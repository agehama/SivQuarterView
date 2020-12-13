#include <Siv3D.hpp> // OpenSiv3D v0.4.3
#include "QuarterView.hpp"

void Main()
{
	Window::Resize(800, 800);
	Scene::SetBackground(ColorF(0.95));
	Font font(64);

	const Color colorX(U"#f26d6d");
	const Color colorY(U"#62e762");
	const Color colorZ(U"#67acf2");

	QuarterView quarterView(Scene::Center());

	constexpr Size textureSize(300, 300);
	auto pLayerX = quarterView.newLayer(textureSize, LayerType::X);
	auto pLayerY = quarterView.newLayer(textureSize, LayerType::Y);
	auto pLayerZ = quarterView.newLayer(textureSize, LayerType::Z);

	while (System::Update())
	{
		quarterView.update();

		{
			auto r = pLayerX->render();

			font(U"X平面").drawAt(r.center(), colorX);

			RectF(50, 50).setPos(Arg::bottomRight = r.bottomRight()).draw(colorX);
			r.rect().drawFrame(10, colorX);
		}

		{
			auto r = pLayerY->render();

			font(U"Y平面").drawAt(r.center(), colorY);

			RectF(50, 50).setPos(Vec2::Zero()).draw(colorY);
			r.rect().drawFrame(10, colorY);
		}

		{
			auto r = pLayerZ->render();

			font(U"Z平面").drawAt(r.center(), colorZ);

			RectF(50, 50).setPos(Arg::bottomLeft = r.bottomLeft()).draw(colorZ);
			r.rect().drawFrame(10, colorZ);
		}

		quarterView.draw();
	}
}
