# SivQuarterView
<img width="400" alt="ScreenShot" src="https://github.com/agehama/SivQuarterView/blob/resource/20201213-195642-124.png">

OpenSiv3D(v0.4.3) でクォータービューを実装するためのシングルヘッダライブラリです。

## 使い方

```cpp:sample1_hello.cpp
#include <Siv3D.hpp> // OpenSiv3D v0.4.3
#include "QuarterView.hpp"

void Main()
{
	Window::Resize(400, 300);
	QuarterView quarterView(Scene::Center());

	// 解像度と平面の向きを指定して描画先となるレイヤーを取得する
	auto pLayer = quarterView.newLayer(Size(800, 600), LayerType::Y);

	while (System::Update())
	{
		quarterView.update();

		{
			auto r = pLayer->render();

			/*
			* ここで通常の2D描画を行うとクォータービュー視点に変換されて描画される
			*/
			SimpleGUI::Headline(U"Hello!", Vec2::Zero());
		}

		quarterView.draw();
	}
}
```
