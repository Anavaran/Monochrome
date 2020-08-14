#pragma once
#include <Monochrome.h>
using namespace mc;

enum class Widget
{
	Unknown,
	Label,
	Button,
	Checkbox,
	Slider
};

namespace utils
{
	std::string ColorToString(Color color);
	Color StringToColor(const std::string& str);

	template <typename T>
	bool CheckType(UIView* view)
	{
		return dynamic_cast<T*>(view);
	}

	class EditorSettings
	{
	public:
		static bool			GridSnapping;
		static uint32_t		GridSize;

		static bool			ProjectAutoSaving;
		static uint32_t		ProjectAutoSavingInterval;
	};
};
