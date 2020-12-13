#pragma once
#include <Siv3D.hpp> // OpenSiv3D v0.4.3

template<class T>
class Transitional
{
public:
	Transitional() = default;
	Transitional(const T& value) :value(value) {}

	bool isMoving()const
	{
		return transitionWatch.isRunning();
	}

	const T& getValue()const
	{
		return value;
	}

	void setValue(const T& newValue, bool force = true)
	{
		value = newValue;
		if (force)
		{
			targetValue = newValue;
			transitionWatch.reset();
		}
	}

	void setTargetValue(const T& newValue, int32 milliSec = 200, std::function<double(double)> func = EaseOutCirc)
	{
		if (targetValue == newValue)
		{
			return;
		}
		oldValue = value;
		targetValue = newValue;
		transitionMilliSec = milliSec;
		transitionFunc = func;
		transitionWatch.restart();
	}

	void update()
	{
		if (transitionWatch.isRunning())
		{
			if (transitionMilliSec <= transitionWatch.ms())
			{
				value = targetValue;
				transitionWatch.reset();
			}
			else
			{
				const double progress = 1.0 * transitionWatch.ms() / transitionMilliSec;
				value = Math::Lerp(oldValue, targetValue, transitionFunc(progress));
			}
		}
	}

private:
	T value;

	Stopwatch transitionWatch;
	T oldValue, targetValue;
	int32 transitionMilliSec;
	std::function<double(double)> transitionFunc;
};

enum class LayerType { Z, X, Y };
enum class LayerAlignPos { TopLeft, TopRight, BottomLeft, BottomRight, LeftCenter, TopCenter, RightCenter, BottomCenter, Center };

class QuarterLayer;
using QuarterLayerPtr = std::shared_ptr<QuarterLayer>;

class QuarterView
{
public:

	QuarterView() = default;

	QuarterView(const Vec2& origin) :origin(origin) {}

	void update();

	void draw();
	void drawPartial(int32 beginGroupIndex, size_t drawGroupCount = -1);

	void resolve();

	QuarterLayerPtr newLayer(const Size& resolution, LayerType type = LayerType::Z, double elevation = 0.0, const Vec2& position = Vec2::Zero())
	{
		layers.push_back(std::make_shared<QuarterLayer>(*this, type, resolution, position, elevation, Vec2::One()));
		return layers.back();
	}

	void erase(QuarterLayerPtr eraseLayer)
	{
		layers.erase(std::remove_if(layers.begin(), layers.end(), [&](QuarterLayerPtr p) { return p == eraseLayer; }), layers.end());
	}

	Vec2 vectorX()const { return Vec2(Math::Cos(angleAxisX), Math::Sin(angleAxisX)); }

	Vec2 vectorY()const { return Vec2(0, -1); }

	Vec2 vectorZ()const { return Vec2(-Math::Cos(angleAxisZ), Math::Sin(angleAxisZ)); }

	Quad screenQuad(const QuarterLayer& layer)const;

	Quad screenQuad(LayerType LayerType, LayerAlignPos alignType, const Size& layerSize, const Vec2& position, double elevation, const Vec2& scale)const;

	Vec2 screenCenter(const QuarterLayer& layer)const;
	Vec2 screenPos(const QuarterLayer& layer, LayerAlignPos focusPos)const;

	void focus(const QuarterLayer& layer);
	void focus(const QuarterLayer& layer, LayerAlignPos focusPos);

	void setAngle(double angle)
	{
		angleAxisX = angle;
		angleAxisZ = angle;
	}

	double angleAxisX = 30_deg;
	double angleAxisZ = 30_deg;

	Vec2 origin = Vec2::Zero();

private:
	std::vector<QuarterLayerPtr> layers;
};

template<class TransformerObj>
struct LayerRegion
{
	LayerRegion(const Rect& region, TransformerObj&& transformer):
		region(region),
		transformer(std::move(transformer))
	{}

	const Rect& rect()const { return region; }

	RectF rectF()const { return region; }

	Vec2 topLeft()const { return region.tl(); }

	Vec2 topRight()const { return region.tr(); }

	Vec2 bottomLeft()const { return region.bl(); }

	Vec2 bottomRight()const { return region.br(); }

	Vec2 center()const { return region.center(); }

private:
	TransformerObj transformer;
	Rect region;
};

class QuarterLayer
{
public:

	QuarterLayer(const QuarterView& quarterView, LayerType type, const Size& size, const Vec2& position, double elevation, const Vec2& scale) :
		quarterViewRef(quarterView),
		type(type),
		texture(size),
		scale(scale)
	{
		setBackground(backGroundColor);

		const LayerAlignPos defaultAlignments[] = { LayerAlignPos::BottomLeft, LayerAlignPos::BottomRight, LayerAlignPos::TopLeft };
		alignPos = defaultAlignments[static_cast<int32>(type)];

		setPosition(position);
		setElevation(elevation);
	}

	LayerRegion<std::tuple<ScopedRenderTarget2D, ScopedRenderStates2D, Transformer2D>> render(bool clearColor = true, bool transformCursor = true)
	{
		rendered = true;
		if (clearColor)
		{
			texture.clear(backGroundColor);
		}
		resolved = false;

		const auto mat = getMat();
		return LayerRegion<std::tuple<ScopedRenderTarget2D, ScopedRenderStates2D, Transformer2D>>(
			Rect(texture.size()),
			std::tuple<ScopedRenderTarget2D, ScopedRenderStates2D, Transformer2D>(
				ScopedRenderTarget2D(texture),
				BlendState(true, Blend::SrcAlpha, Blend::InvSrcAlpha, BlendOp::Add, Blend::One, Blend::InvSrcAlpha),
				Transformer2D(Mat3x2::Identity(), transformCursor ? mat : Mat3x2::Identity())
				)
			);
	}

	LayerRegion<Transformer2D> renderDirectly(bool transformCursor = true)const
	{
		const auto mat = getMat();
		return LayerRegion<Transformer2D>(
			Rect(texture.size()),
			Transformer2D(mat, transformCursor ? mat : Mat3x2::Identity())
			);
	}

	int32 width()const { return texture.width(); }

	int32 height()const { return texture.height(); }

	Size size()const { return texture.size(); }

	void setBackground(Color color)
	{
		backGroundColor = color;
		resolved = false;
		texture.clear(backGroundColor);
	}

	bool isResolved()const { return resolved; }

	bool isRendered()const { return rendered; }

	Mat3x2 getMat()const
	{
		const auto& quarterView = quarterViewRef.get();
		const double angleAxisX = quarterView.angleAxisX;
		const double angleAxisZ = quarterView.angleAxisZ;
		const Vec2& screenOrigin = quarterView.origin;

		const double s = Math::Sqrt(Math::Tan(angleAxisX) * Math::Tan(angleAxisZ));
		const double theta = Math::Atan(Math::Tan(angleAxisX) / s);
		const double s2 = Math::Cos(angleAxisX) / Math::Cos(theta);
		switch (type)
		{
		case LayerType::Z:
			return
				//原点をテクスチャ左下に合わせる
				BaseTranslate(alignPos, texture.size())
				.scaled(scale.getValue())
				.translated(getPosition())
				//shearedYで引き延ばされるscaleの補正
				.scaled(Math::Cos(angleAxisX), 1.0)
				//角度をずらす
				.shearedY(Math::Tan(angleAxisX))
				//elevation
				.translated(Vec2(-Math::Cos(angleAxisZ), Math::Sin(angleAxisZ)) * getElevation() + screenOrigin);
		case LayerType::X:
			return
				//原点をテクスチャ右下に合わせる
				BaseTranslate(alignPos, texture.size())
				.scaled(scale.getValue())
				.translated(getPosition())
				//shearedYで引き延ばされるscaleの補正
				.scaled(Math::Cos(angleAxisZ), 1.0)
				//XZ平面と同じスケールに揃える
				.scaled(Math::Tan(theta) * Math::Cos(angleAxisX) / Math::Cos(angleAxisZ), 1.0)
				//角度をずらす
				.shearedY(Math::Tan(-angleAxisZ))
				//elevation
				.translated(Vec2(Math::Cos(angleAxisX), Math::Sin(angleAxisX)) * getElevation() + screenOrigin);
		case LayerType::Y:
		{
			return
				BaseTranslate(alignPos, texture.size())
				.scaled(scale.getValue())
				.translated(getPosition())
				.rotated(theta)
				.scaled(Vec2(1, s) * s2)
				.translated(Vec2(0, -getElevation()) + screenOrigin);
		}
		default: return Mat3x2::Identity();
		}
	}

	static Mat3x2 GetMat(double angleAxisX, double angleAxisZ, const Vec2& screenOrigin, LayerType type, LayerAlignPos alignType, const Size& textureSize, const Vec2& position, const Vec2& scale, double elevation)
	{
		const double s = Math::Sqrt(Math::Tan(angleAxisX) * Math::Tan(angleAxisZ));
		const double theta = Math::Atan(Math::Tan(angleAxisX) / s);
		const double s2 = Math::Cos(angleAxisX) / Math::Cos(theta);
		switch (type)
		{
		case LayerType::Z:
			return
				//原点をテクスチャ左下に合わせる
				BaseTranslate(alignType, textureSize)
				.scaled(scale)
				.translated(position)
				//shearedYで引き延ばされるscaleの補正
				.scaled(Math::Cos(angleAxisX), 1.0)
				//角度をずらす
				.shearedY(Math::Tan(angleAxisX))
				//elevation
				.translated(Vec2(-Math::Cos(angleAxisZ), Math::Sin(angleAxisZ)) * elevation + screenOrigin);
		case LayerType::X:
			return
				//原点をテクスチャ右下に合わせる
				BaseTranslate(alignType, textureSize)
				.scaled(scale)
				.translated(position)
				//shearedYで引き延ばされるscaleの補正
				.scaled(Math::Cos(angleAxisZ), 1.0)
				//XZ平面と同じスケールに揃える
				.scaled(Math::Tan(theta) * Math::Cos(angleAxisX) / Math::Cos(angleAxisZ), 1.0)
				//角度をずらす
				.shearedY(Math::Tan(-angleAxisZ))
				//elevation
				.translated(Vec2(Math::Cos(angleAxisX), Math::Sin(angleAxisX)) * elevation + screenOrigin);
		case LayerType::Y:
		{
			return
				BaseTranslate(alignType, textureSize)
				.scaled(scale)
				.translated(position)
				.rotated(theta)
				.scaled(Vec2(1, s) * s2)
				.translated(Vec2(0, -elevation) + screenOrigin);
		}
		default: return Mat3x2::Identity();
		}
	}

	Vec2 getPosition()const { return Vec2(get2DPositionX(), get2DPositionY()); }
	void setPosition(const Vec2& newPosition, bool force = true)
	{
		set2DPositionX(newPosition.x, force);
		set2DPositionY(newPosition.y, force);
	}
	void setTargetPosition(const Vec2& newPosition, int32 transitionMilliSec = 200, std::function<double(double)> transitionFunc = EaseOutCirc)
	{
		setTarget2DPositionX(newPosition.x, transitionMilliSec, transitionFunc);
		setTarget2DPositionY(newPosition.y, transitionMilliSec, transitionFunc);
	}
	bool isPositionMoving()const { return is2DPositionMoving(); }
	
	double getElevation()const
	{
		return elevationValue().getValue();
	}
	void setElevation(double newElevation, bool force = true)
	{
		elevationValue().setValue(newElevation, force);
	}
	void setTargetElevation(double newElevation, int32 transitionMilliSec = 200, std::function<double(double)> transitionFunc = EaseOutCirc)
	{
		elevationValue().setTargetValue(newElevation, transitionMilliSec, transitionFunc);
	}
	bool isElevationMoving()const
	{
		return elevationValue().isMoving();
	}
	
	const Vec2& getScale()const { return scale.getValue(); }
	void setScale(const Vec2& newScale, bool force = true)
	{
		scale.setValue(newScale, force);
	}
	void setTargetScale(const Vec2& newScale, int32 transitionMilliSec = 200, std::function<double(double)> transitionFunc = EaseOutCirc)
	{
		scale.setTargetValue(newScale, transitionMilliSec, transitionFunc);
	}
	bool isScaleMoving()const { return scale.isMoving(); }

	Vec3 get3DPosition()const
	{
		return Vec3(_x.getValue(), _y.getValue(), _z.getValue());
	}
	void set3DPosition(const Vec3& newPosition)
	{
		_x.setValue(newPosition.x);
		_y.setValue(newPosition.y);
		_z.setValue(newPosition.z);
	}
	void setTarget3DPosition(const Vec3& newPosition, int32 transitionMilliSec = 200, std::function<double(double)> transitionFunc = EaseOutCirc)
	{
		_x.setTargetValue(newPosition.x, transitionMilliSec, transitionFunc);
		_y.setTargetValue(newPosition.y, transitionMilliSec, transitionFunc);
		_z.setTargetValue(newPosition.z, transitionMilliSec, transitionFunc);
	}
	bool is3DPositionMoving()const { return _x.isMoving() || _y.isMoving() || _z.isMoving(); }

	LayerType type;
	LayerAlignPos alignPos;
	int32 drawGroup = 0;
	
	MSRenderTexture texture;

private:

	void update()
	{
		rendered = false;

		_x.update();
		_y.update();
		_z.update();
		scale.update();
	}

	bool is2DPositionMoving()const
	{
		switch (type)
		{
		case LayerType::Z:
			return _x.isMoving() || _y.isMoving();
		case LayerType::X:
			return _z.isMoving() || _y.isMoving();
		default:
			return _x.isMoving() || _z.isMoving();
		}
	}

	double get2DPositionX()const
	{
		switch (type)
		{
		case LayerType::X:
			return -_z.getValue();
		default:
			return _x.getValue();
		}
	}

	void set2DPositionX(double newPositionX, bool force)
	{
		switch (type)
		{
		case LayerType::X:
			_z.setValue(-newPositionX, force);
			break;
		default:
			_x.setValue(newPositionX, force);
			break;
		}
	}

	void setTarget2DPositionX(double newPositionX, int32 transitionMilliSec, std::function<double(double)> transitionFunc)
	{
		switch (type)
		{
		case LayerType::X:
			_z.setTargetValue(-newPositionX, transitionMilliSec, transitionFunc);
			break;
		default:
			_x.setTargetValue(newPositionX, transitionMilliSec, transitionFunc);
			break;
		}
	}

	double get2DPositionY()const
	{
		switch (type)
		{
		case LayerType::Y:
			return _z.getValue();
		default:
			return -_y.getValue();
		}
	}

	void set2DPositionY(double newPositionY, bool force)
	{
		switch (type)
		{
		case LayerType::Y:
			_z.setValue(newPositionY, force);
			break;
		default:
			_y.setValue(-newPositionY, force);
		}
	}

	void setTarget2DPositionY(double newPositionY, int32 transitionMilliSec, std::function<double(double)> transitionFunc)
	{
		switch (type)
		{
		case LayerType::Y:
			_z.setTargetValue(newPositionY, transitionMilliSec, transitionFunc);
			break;
		default:
			_y.setTargetValue(-newPositionY, transitionMilliSec, transitionFunc);
		}
	}

	static Mat3x2 BaseTranslate(LayerAlignPos alignPos, const Size& textureSize)
	{
		switch (alignPos)
		{
		case LayerAlignPos::TopLeft:		return Mat3x2::Identity();
		case LayerAlignPos::TopRight:		return Mat3x2::Translate(-textureSize.x, 0);
		case LayerAlignPos::BottomRight:	return Mat3x2::Translate(-textureSize.x, -textureSize.y);
		case LayerAlignPos::BottomLeft:		return Mat3x2::Translate(0, -textureSize.y);
		case LayerAlignPos::LeftCenter:		return Mat3x2::Translate(0, -textureSize.y * 0.5);
		case LayerAlignPos::TopCenter:		return Mat3x2::Translate(-textureSize.x * 0.5, 0);
		case LayerAlignPos::RightCenter:	return Mat3x2::Translate(-textureSize.x, -textureSize.y * 0.5);
		case LayerAlignPos::BottomCenter:	return Mat3x2::Translate(-textureSize.x * 0.5, -textureSize.y);
		case LayerAlignPos::Center:			return Mat3x2::Translate(-textureSize.x * 0.5, -textureSize.y * 0.5);
		default:							return Mat3x2::Identity();
		}
	}

	Transitional<double>& elevationValue()
	{
		switch (type)
		{
		case LayerType::Z:
			return _z;
		case LayerType::X:
			return _x;
		default:
			return _y;
		}
	}

	const Transitional<double>& elevationValue()const
	{
		switch (type)
		{
		case LayerType::Z:
			return _z;
		case LayerType::X:
			return _x;
		default:
			return _y;
		}
	}

	void resolve()
	{
		if (!resolved)
		{
			texture.resolve();
			resolved = true;
		}
	}

	friend class QuarterView;

	std::reference_wrapper<const QuarterView> quarterViewRef;

	Color backGroundColor = Alpha(0);
	bool resolved = true;
	bool rendered = false;

	Transitional<double> _x, _y, _z;
	Transitional<Vec2> scale;
};

inline void QuarterView::update()
{
	for (auto& pLayer : layers)
	{
		pLayer->update();
	}
}

inline void QuarterView::resolve()
{
	if (std::any_of(layers.begin(), layers.end(), [](const QuarterLayerPtr& layer) { return !layer->isResolved(); }))
	{
		Graphics2D::Flush();
		for (auto& pLayer : layers)
		{
			pLayer->resolve();
		}
	}
}

inline void QuarterView::draw()
{
	resolve();

	const auto getI = [&](size_t index)->QuarterLayer&
	{
		auto it = layers.begin();
		for (size_t i = 0; i < index; ++i)
		{
			++it;
		}
		return *it->get();
	};

	using IndexElevation = std::pair<size_t, double>;

	const auto minMaxElevation = std::minmax_element(layers.begin(), layers.end(), [](QuarterLayerPtr a, QuarterLayerPtr b) { return a->getElevation() < b->getElevation(); });
	const double minElevation = (*minMaxElevation.first)->getElevation();
	const double maxElevation = (*minMaxElevation.second)->getElevation();

	auto indices = Array<IndexElevation>::IndexedGenerate(layers.size(),
		[&](size_t i) {
			auto& layer = getI(i);
			return IndexElevation(i, layer.drawGroup + Math::InvLerp(minElevation, maxElevation, layer.getElevation()));
		}
	).filter([&](const IndexElevation& index) { return getI(index.first).isRendered(); }
	);

	indices.sort_by([&](const IndexElevation& a, const IndexElevation& b) { return a.second < b.second; });

	for (const auto& index : indices)
	{
		const QuarterLayer& layer = getI(index.first);
		//auto t = createTransformer2D(layer);
		auto t = layer.renderDirectly(false);
		layer.texture.draw();
	}
}

inline void QuarterView::drawPartial(int32 beginGroupIndex, size_t drawGroupCount)
{
	resolve();

	const auto getI = [&](size_t index)->QuarterLayer&
	{
		auto it = layers.begin();
		for (size_t i = 0; i < index; ++i)
		{
			++it;
		}
		return *it->get();
	};

	using IndexElevation = std::pair<size_t, double>;

	const auto minMaxElevation = std::minmax_element(layers.begin(), layers.end(), [](QuarterLayerPtr a, QuarterLayerPtr b) { return a->getElevation() < b->getElevation(); });
	const double minElevation = (*minMaxElevation.first)->getElevation();
	const double maxElevation = (*minMaxElevation.second)->getElevation();

	auto indices = Array<IndexElevation>::IndexedGenerate(layers.size(),
		[&](size_t i) {
			auto& layer = getI(i);
			return IndexElevation(i, layer.drawGroup + Math::InvLerp(minElevation, maxElevation, layer.getElevation()));
		}
	).filter([&](const IndexElevation& index) { const int32 groupIndex = getI(index.first).drawGroup; return getI(index.first).isRendered() && beginGroupIndex <= groupIndex && (drawGroupCount == -1 || groupIndex < beginGroupIndex + static_cast<int32>(drawGroupCount)); }
	);

	indices.sort_by([&](const IndexElevation& a, const IndexElevation& b) { return a.second < b.second; });

	for (const auto& index : indices)
	{
		const QuarterLayer& layer = getI(index.first);
		//auto t = createTransformer2D(layer);
		auto t = layer.renderDirectly(false);
		layer.texture.draw();
	}
}

inline Quad QuarterView::screenQuad(const QuarterLayer& layer)const
{
	const auto ps = Array<Vec2>({ Vec2(0, 0), Vec2(layer.width(), 0), Vec2(layer.size()), Vec2(0, layer.height()) })
		.map([&](const Vec2& p) { return layer.getMat().transform(p); });
	return Quad(ps[0], ps[1], ps[2], ps[3]);
}

inline Quad QuarterView::screenQuad(LayerType LayerType, LayerAlignPos alignType, const Size& layerSize, const Vec2& position, double elevation, const Vec2& scale)const
{
	const auto quarterMat = QuarterLayer::GetMat(angleAxisX, angleAxisZ, origin, LayerType, alignType, layerSize, position, scale, elevation);
	const auto ps = Array<Vec2>({ Vec2(0, 0), Vec2(layerSize.x, 0), Vec2(layerSize.x, layerSize.y), Vec2(0, layerSize.y) })
		.map([&](const Vec2& p) { return quarterMat.transform(p); });
	return Quad(ps[0], ps[1], ps[2], ps[3]);
}

inline Vec2 QuarterView::screenCenter(const QuarterLayer& layer)const
{
	return layer.getMat().transform(layer.size() * 0.5);
}

inline Vec2 QuarterView::screenPos(const QuarterLayer& layer, LayerAlignPos focusPos)const
{
	const auto mat = layer.getMat();
	switch (focusPos)
	{
	case LayerAlignPos::TopLeft:
		return mat.transform(Vec2(0, 0));
	case LayerAlignPos::TopRight:
		return mat.transform(Vec2(layer.width(), 0));
	case LayerAlignPos::BottomLeft:
		return mat.transform(Vec2(0, layer.height()));
	case LayerAlignPos::BottomRight:
		return mat.transform(layer.size());
	case LayerAlignPos::LeftCenter:
		return mat.transform(Vec2(0, layer.height() * 0.5));
	case LayerAlignPos::TopCenter:
		return mat.transform(Vec2(layer.width() * 0.5, 0));
	case LayerAlignPos::RightCenter:
		return mat.transform(Vec2(layer.width(), layer.height() * 0.5));
	case LayerAlignPos::BottomCenter:
		return mat.transform(Vec2(layer.width() * 0.5, layer.height()));
	case LayerAlignPos::Center:
		return mat.transform(layer.size() * 0.5);
		break;
	default:
		return Vec2::Zero();
	}
}

inline void QuarterView::focus(const QuarterLayer& layer)
{
	const Vec2 toOriginFromCenter = origin - screenCenter(layer);
	origin = Scene::Center() + toOriginFromCenter;
}

inline void QuarterView::focus(const QuarterLayer& layer, LayerAlignPos focusPos)
{
	const Vec2 toOriginFromCenter = origin - screenPos(layer, focusPos);
	origin = Scene::Center() + toOriginFromCenter;
}
