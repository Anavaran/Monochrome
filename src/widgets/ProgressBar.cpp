#include  "ProgressBar.h"
#include  <rendering/Renderer.h>

namespace mc {
	ProgressBar::ProgressBar() {
		_setupProperties();
		appendAllowedEvent("taskCompleted");
	}

	void ProgressBar::onRender(Shared<RenderTarget>& renderTarget, Position& parentPositionOffset, getWidgetBoundsFn_t getWidgetBounds)
	{

		auto [position, size] = getWidgetBounds(this, parentPositionOffset);

		renderTarget->drawRectangle(
			position.x, position.y,
			size.width, size.height,
			Color::black,
			0,
			true,
			0
		);

		renderTarget->drawRectangle(
			position.x, position.y,
			size.width - progression, size.height,
			Color::red,
			0,
			true,
			0
		);
	}

	void ProgressBar::_setupProperties() {

		borderSize = 2;
		borderColor.forwardEmittedEvents(this);

		borderColor = Color::red;
		borderColor.forwardEmittedEvents(this);
	}
}