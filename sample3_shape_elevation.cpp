#include <Siv3D.hpp> // OpenSiv3D v0.4.3
#include "QuarterView.hpp"

void Main()
{
	Window::Resize(1000, 1000);
	Scene::SetBackground(Color(U"#cdd3d7"));
	Font font(20, Typeface::Heavy);

	QuarterView quarterView(Scene::Center());

	constexpr Size textureSize(150, 150);
	auto layers = Array<QuarterLayerPtr>::IndexedGenerate(9,
		[&](size_t i) { return quarterView.newLayer(textureSize, LayerType::Y, 0.0, Vec2(i % 3, i / 3) * 200.0); });

	const int margin = 100;
	const auto gridLayerSize = Point::One() * (200 * 2 + 150);
	auto pLayerFloor = quarterView.newLayer(gridLayerSize + Size(margin, margin) * 2, LayerType::Y);
	pLayerFloor->drawGroup = -1;
	pLayerFloor->setPosition(Vec2(-margin, -margin));

	quarterView.focus(*pLayerFloor);

	const auto drawLayerFrame = [&](const String& name)
	{
		Rect(textureSize).drawFrame(10, Color(240));
		const Point textPos(5, 0);
		const auto text = font(name);
		text.region().moveBy(textPos).stretched(4).draw(Color(240));
		text.draw(textPos + Vec2::One() * 1.5, Palette::Gray);
		text.draw(textPos, Palette::Black);
	};

	const Array<Polygon> shapes({
			Shape2D::Arrow(Vec2(-30, 0),Vec2(30, 0), 20, Vec2(20, 20)),
			Shape2D::Cross(30, 10),
			Shape2D::Hexagon(30),
			Shape2D::Pentagon(30),
			Shape2D::Plus(30, 10),
			Shape2D::RectBalloon(RectF(60, 40).setCenter(Vec2::Zero()),Vec2(30, 40)),
			Shape2D::Rhombus(30, 60),
			Shape2D::Stairs(Vec2::Zero(), 60, 60, 5),
			Shape2D::Star(30)
		}
	);

	const double maxElevation = 50.0;

	while (System::Update())
	{
		quarterView.update();

		for (auto [i, pLayer] : Indexed(layers))
		{
			const double elevation01 = Math::InvLerp(0.0, maxElevation, pLayer->getElevation());

			// Elevation が動いている途中でなければ一定確率で動かす
			if (!pLayer->isElevationMoving() && RandomBool(0.01))
			{
				// 目標とする Elevation を設定してその値まで 400ms かけてイージングしながら遷移する
				pLayer->setTargetElevation(0.5 < elevation01 ? 0.0 : maxElevation, 400);
			}

			auto r = pLayer->render();

			r.rect().draw(HSV(20 * i, 0.8, 0.8, elevation01));
			shapes[i].movedBy(-shapes[i].centroid() + r.rect().center()).draw(HSV(20 * i, 0.3, 0.4).lerp(Palette::White, elevation01));

			drawLayerFrame(Format(U"Layer", i));
		}

		{
			auto r = pLayerFloor->render();

			r.rect().drawFrame(4.0, Color(50));

			for (auto pLayer : layers)
			{
				RectF(pLayer->size()).moveBy(pLayer->getPosition() + Vec2(margin, margin)).draw(Color(U"#e6e8e7"));
			}
		}

		quarterView.draw();
	}
}
