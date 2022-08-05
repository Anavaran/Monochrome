#pragma once
#include "CustomRenderedWidget.h"

namespace mc {
class ProgressBar : public CustomRenderedWidget {
public:
	ProgressBar();

	//Width of the boarder around the progress bar's body
	PropertyObserver<uint32_t>      borderSize;

	//Color of the progress bar border
	PropertyObserver<Color>         borderColor;

	//Fill in the progress bar
	PropertyObserver<Color>         progressColor;

	//progression value 
	PropertyObserver<uint32_t>      progression;

    //Minimum value
	PropertyObserver<int32_t>       minValue;

	//Maximum value
	PropertyObserver<int32_t>       maxValue;

	//Current progress
	PropertyObserver<int32_t>       value;



	void onRender(
		Shared<RenderTarget>& renderTarget,
		Position& parentPositionOffset,
		getWidgetBoundsFn_t getWidgetBounds
	) override;

private:
	void _setupProperties() override;
};



}