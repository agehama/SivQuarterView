#include <Siv3D.hpp> // OpenSiv3D v0.4.3
#include "QuarterView.hpp"

double WaveFunc(double x_, double z_)
{
	const double height = 30.0;
	const double width = 3.0;
	const double x = x_ * width;
	const double z = z_ * width;

	return height * (Math::Cos(x + z) + Math::Sin(Math::Cos(2 * x + 3 * z) + x - z));
}

Array<Vec2> FunctionPoints(const Rect& rect, double xBegin, double xEnd, double z, std::function<double(double, double)> f)
{
	const int divNum = 100;

	return Array<Vec2>::IndexedGenerate(divNum,
		[&](size_t i)
		{
			const double t = 1.0 * i / (divNum - 1);
			return Math::Lerp(rect.leftCenter(), rect.rightCenter(), t) + Vec2(0, f(Math::Lerp(xBegin, xEnd, t), z));
		}
	);
}

void Main()
{
	Window::Resize(1000, 1000);
	Scene::SetBackground(Color(U"#204b57"));

	QuarterView quarterView(Scene::Center());

	constexpr Size textureSize(300, 300);
	const int layerSize = 40;
	auto layersZ = Array<QuarterLayerPtr>::IndexedGenerate(layerSize,
		[&](size_t i) { return quarterView.newLayer(textureSize, LayerType::Z, i * 300.0 / (layerSize - 1)); });

	Array<QuarterLayerPtr> layersX({
		quarterView.newLayer(textureSize, LayerType::X, 0.0),
		quarterView.newLayer(textureSize, LayerType::X, 300.0)
		}
	);

	const auto getColor = [](double z) { return HSV(160 + 120 * z, 0.4, 0.7); };

	Stopwatch watch(true);
	while (System::Update())
	{
		quarterView.update();

		const double shift = 0.2 * watch.sF();
		const double xShift = shift * 0.5;
		const double zShift = shift * 0.7;

		for (auto [layerIndex, pLayer] : Indexed(layersZ))
		{
			auto r = pLayer->render();

			const double t = 1.0 * layerIndex / (layersZ.size() - 1);
			auto points = FunctionPoints(r.rect(), xShift, xShift + 1, zShift - t, [](double x, double z) { return WaveFunc(x, z); });

			Polygon(points.append(Array<Vec2>{r.rect().br(), r.rect().bl()})).draw(getColor(t));
		}

		for (auto [layerIndex, pLayer] : Indexed(layersX))
		{
			auto r = pLayer->render();

			const auto points = FunctionPoints(r.rect(), zShift - 1, zShift, xShift + layerIndex, [](double x, double z) { return WaveFunc(z, x); });

			const double bottomY = r.bottomLeft().y;
			for (size_t i = 0; i + 1 < points.size(); i++)
			{
				const Color c0(getColor(1.0 - 1.0 * i / (points.size() - 1)), 200);
				const Color c1(getColor(1.0 - 1.0 * (i + 1) / (points.size() - 1)), 200);
				const Vec2& p0 = points[i];
				const Vec2& p1 = points[i + 1];
				Quad(p0, p1, Vec2(p1.x, bottomY), Vec2(p0.x, bottomY)).draw(c0, c1, c1, c0);
			}
		}

		quarterView.draw();
	}
}