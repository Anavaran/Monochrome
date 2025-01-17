#include "ProjectGenerator.h"
#include <fstream>

#include "../rapidxml/rapidxml.hpp"
#include "../rapidxml/rapidxml_print.hpp"
using namespace rapidxml;

namespace utils
{
#define MC_ENUM_ELEM_TO_STRING(elem) "mc::"##elem

    static std::wstring StringToWidestring(const std::string& as)
    {
        if (as.empty())    return std::wstring();

        size_t reqLength = ::MultiByteToWideChar(CP_UTF8, 0, as.c_str(), (int)as.length(), 0, 0);
        std::wstring ret(reqLength, L'\0');

        ::MultiByteToWideChar(CP_UTF8, 0, as.c_str(), (int)as.length(), &ret[0], (int)ret.length());
        return ret;
    }

    void CheckAndResolveViewNodeType(xml_node<>* node, Ref<UIView>* outValue);
    Widget GetWidgetType(UIView* view);
    std::string WidgetTypeToString(Widget type);
    void AddWidgetPropertyNodes(Ref<xml_document<>>& doc, xml_node<>*& view_node, Widget type, Ref<UIView>& view);

    void AddBasicPropertyNodes(Ref<xml_document<>>& doc, xml_node<>*& view_node, Ref<UIView>& view);
    void AddLabelPropertyNodes(Ref<xml_document<>>& doc, xml_node<>*& view_node, Ref<UILabel>& label);
    void AddButtonPropertyNodes(Ref<xml_document<>>& doc, xml_node<>*& view_node, Ref<UIButton>& button);
    void AddCheckboxPropertyNodes(Ref<xml_document<>>& doc, xml_node<>*& view_node, Ref<UICheckbox>& checkbox);
    void AddSliderPropertyNodes(Ref<xml_document<>>& doc, xml_node<>*& view_node, Ref<UISlider>& slider);

    void UIViewToXML(Ref<xml_document<>>& doc, xml_node<>*& root_node, Ref<UIView>& view, std::map<Ref<UIView>, ElementCodeProperties>& element_code_props);
    void AddWindowSettingsNode(Ref<xml_document<>>& doc, xml_node<>*& root_node, WindowSettings& window_settings);

    void ProjectGenerator::GenerateProjectAndVisualStudioSolution(ProjectConfig& config)
    {
        // Create monochrome UI layout file
        CreateMCLayoutFile(config.location + "\\" + config.projectName + ".mc", config.uiViews, config.windowSettings, *config.elementCodeProperties);

        // Arg1 --> Should start CMake process
        // Arg2 --> Target Path
        // Arg3 --> Project Name
        // Arg4 --> Class Name
        // Arg5 --> Monochrome Source Path
        // Arg6 --> Monochrome Library Debug Path
        // Arg7 --> Monochrome Library Release Path

        std::string cmd = std::string("python project_source_generator.py 1 " +
            std::string("\"") + config.location + "\"" + " \"" + config.projectName + "\" \"" + config.uiClassName + "\" " +
            "\"" + config.monochromeSourcePath + "\" \"" + config.monochromeLibraryDebugPath + "\" \"" + config.monochromeLibraryReleasePath + "\""
        );

        std::system(cmd.c_str());
    }

    void ProjectGenerator::GenerateProjectSourceFiles(ProjectConfig& config)
    {
        // Create monochrome UI layout file
        CreateMCLayoutFile(config.location + "\\" + config.projectName + ".mc", config.uiViews, config.windowSettings, *config.elementCodeProperties);

        // Arg1 --> Should start CMake process
        // Arg2 --> Target Path
        // Arg3 --> Project Name
        // Arg4 --> Class Name
        // Arg5 --> Monochrome Source Path
        // Arg6 --> Monochrome Library Debug Path
        // Arg7 --> Monochrome Library Release Path

        std::string cmd = std::string("python project_source_generator.py 0 " +
            std::string("\"") + config.location + "\"" + " \"" + config.projectName + "\" \"" + config.uiClassName + "\" " +
            "\"" + config.monochromeSourcePath + "\" \"" + config.monochromeLibraryDebugPath + "\" \"" + config.monochromeLibraryReleasePath + "\""
        );

        WinExec(cmd.c_str(), SW_HIDE);
    }

    void ProjectGenerator::CreateMCLayoutFile(std::string& path, std::vector<Ref<UIView>>& views, WindowSettings& window_settings, std::map<Ref<UIView>, ElementCodeProperties>& element_code_props)
    {
        std::ofstream file(path, std::ofstream::trunc);
        Ref<xml_document<>> document = MakeRef<xml_document<>>();

        // Declaration
        xml_node<>* decl = document->allocate_node(node_declaration);
        decl->append_attribute(document->allocate_attribute("version", "1.0"));
        decl->append_attribute(document->allocate_attribute("encoding", "utf-8"));
        document->append_node(decl);

        // Root node
        xml_node<>* root_node = document->allocate_node(node_element, "mcscene");

        AddWindowSettingsNode(document, root_node, window_settings);

        for (auto& view : views)
            UIViewToXML(document, root_node, view, element_code_props);

        document->append_node(root_node);
        file << *document;
        file.close();
    }

    void UIViewToXML(Ref<xml_document<>>& doc, xml_node<>*& root_node, Ref<UIView>& view, std::map<Ref<UIView>, ElementCodeProperties>& element_code_props)
    {
        xml_node<>* view_node = doc->allocate_node(node_element, "uiview");

        Widget widget_type = GetWidgetType(view.get());
        view_node->append_attribute(doc->allocate_attribute("type", doc->allocate_string(WidgetTypeToString(widget_type).c_str())));

        // Element Code Properties
        if (element_code_props.find(view) != element_code_props.end())
        {
            view_node->append_attribute(doc->allocate_attribute("visibility", doc->allocate_string(element_code_props[view].visibility.c_str())));
            view_node->append_attribute(doc->allocate_attribute("name", doc->allocate_string(element_code_props[view].name.c_str())));

            // Event handlers data
            if (element_code_props[view].eventHandlerDataMap.size())
            {
                xml_node<>* event_handlers_data_node = doc->allocate_node(node_element, "event_handlers");
                for (auto& [type, data] : element_code_props[view].eventHandlerDataMap)
                {
                    xml_node<>* handler_node = doc->allocate_node(node_element, "handler");
                    handler_node->append_attribute(doc->allocate_attribute("type", doc->allocate_string(type.c_str())));
                    handler_node->append_attribute(doc->allocate_attribute("generate_member_fn", doc->allocate_string(std::to_string(data.generateClassFunction).c_str())));

                    if (data.generateClassFunction)
                    {
                        xml_node<>* member_fn_name_node = doc->allocate_node(node_element, "name");
                        member_fn_name_node->value(doc->allocate_string(data.classFunctionName.c_str()));

                        xml_node<>* member_fn_visibility_node = doc->allocate_node(node_element, "visibility");
                        member_fn_visibility_node->value(doc->allocate_string(data.classFunctionVisibility.c_str()));

                        xml_node<>* member_fn_node = doc->allocate_node(node_element, "member_fn");
                        member_fn_node->append_node(member_fn_name_node);
                        member_fn_node->append_node(member_fn_visibility_node);
                        handler_node->append_node(member_fn_node);
                    }

                    xml_node<>* return_status_node = doc->allocate_node(node_element, "return_status");
                    if (data.returnStatus == "Handled")
                        return_status_node->value(doc->allocate_string("EVENT_HANDLED"));
                    else
                        return_status_node->value(doc->allocate_string("EVENT_UNHANDLED"));

                    handler_node->append_node(return_status_node);
                    event_handlers_data_node->append_node(handler_node);
                }
                view_node->append_node(event_handlers_data_node);
            }
        }

        AddWidgetPropertyNodes(doc, view_node, widget_type, view);

        for (auto& child : view->subviews)
            UIViewToXML(doc, view_node, child, element_code_props);

        root_node->append_node(view_node);
    }

    Widget GetWidgetType(UIView* view)
    {
        if (CheckType<UILabel>(view))       return Widget::Label;
        if (CheckType<UIButton>(view))      return Widget::Button;
        if (CheckType<UICheckbox>(view))    return Widget::Checkbox;
        if (CheckType<UISlider>(view))      return Widget::Slider;

        return Widget::Unknown;
    }

    std::string WidgetTypeToString(Widget type)
    {
        switch (type)
        {
        case Widget::Label:     return "UILabel";
        case Widget::Button:    return "UIButton";
        case Widget::Checkbox:  return "UICheckbox";
        case Widget::Slider:    return "UISlider";

        default: return "UIView";
        }
    }

    void AddWidgetPropertyNodes(Ref<xml_document<>>& doc, xml_node<>*& view_node, Widget type, Ref<UIView>& view)
    {
        AddBasicPropertyNodes(doc, view_node, view);

        switch (type)
        {
        case Widget::Label:     return AddLabelPropertyNodes(doc, view_node, std::dynamic_pointer_cast<UILabel>(view));
        case Widget::Button:    return AddButtonPropertyNodes(doc, view_node, std::dynamic_pointer_cast<UIButton>(view));
        case Widget::Checkbox:  return AddCheckboxPropertyNodes(doc, view_node, std::dynamic_pointer_cast<UICheckbox>(view));
        case Widget::Slider:    return AddSliderPropertyNodes(doc, view_node, std::dynamic_pointer_cast<UISlider>(view));
        default: break;
        }
    }

    void AddBasicPropertyNodes(Ref<xml_document<>>& doc, xml_node<>*& view_node, Ref<UIView>& view)
    {
        xml_node<>* width_node = doc->allocate_node(node_element, "width");
        width_node->value(doc->allocate_string(std::to_string(view->layer.frame.size.width).c_str()));

        xml_node<>* height_node = doc->allocate_node(node_element, "height");
        height_node->value(doc->allocate_string(std::to_string(view->layer.frame.size.height).c_str()));

        xml_node<>* xpos_node = doc->allocate_node(node_element, "x");
        xpos_node->value(doc->allocate_string(std::to_string(view->layer.frame.position.x).c_str()));

        xml_node<>* ypos_node = doc->allocate_node(node_element, "y");
        ypos_node->value(doc->allocate_string(std::to_string(view->layer.frame.position.y).c_str()));

        xml_node<>* size_node = doc->allocate_node(node_element, "size");
        size_node->append_node(width_node);
        size_node->append_node(height_node);

        xml_node<>* position_node = doc->allocate_node(node_element, "position");
        position_node->append_node(xpos_node);
        position_node->append_node(ypos_node);

        xml_node<>* frame_node = doc->allocate_node(node_element, "frame");
        frame_node->append_node(size_node);
        frame_node->append_node(position_node);

        xml_node<>* color_node = doc->allocate_node(node_element, "color");
        color_node->append_attribute(doc->allocate_attribute("r", doc->allocate_string(std::to_string(view->layer.color.r).c_str())));
        color_node->append_attribute(doc->allocate_attribute("g", doc->allocate_string(std::to_string(view->layer.color.g).c_str())));
        color_node->append_attribute(doc->allocate_attribute("b", doc->allocate_string(std::to_string(view->layer.color.b).c_str())));
        color_node->append_attribute(doc->allocate_attribute("a", doc->allocate_string(std::to_string(view->layer.color.alpha).c_str())));

        xml_node<>* layer_node = doc->allocate_node(node_element, "layer");
        layer_node->append_node(frame_node);
        layer_node->append_node(color_node);
        view_node->append_node(layer_node);

        xml_node<>* zindex_node = doc->allocate_node(node_element, "z_index");
        zindex_node->value(doc->allocate_string(std::to_string(view->GetZIndex()).c_str()));
        view_node->append_node(zindex_node);

        xml_node<>* visible_node = doc->allocate_node(node_element, "visible");
        visible_node->value(doc->allocate_string(std::to_string(view->Visible).c_str()));
        view_node->append_node(visible_node);

        xml_node<>* corner_radius_node = doc->allocate_node(node_element, "corner_radius");
        corner_radius_node->value(doc->allocate_string(std::to_string(view->CornerRadius).c_str()));
        view_node->append_node(corner_radius_node);
    }

    void AddLabelPropertyNodes(Ref<xml_document<>>& doc, xml_node<>*& view_node, Ref<UILabel>& label)
    {
        xml_node<>* text_node = doc->allocate_node(node_element, "text");
        text_node->value(doc->allocate_string(label->Text.c_str()));
        view_node->append_node(text_node);

        xml_node<>* use_widestring_text_node = doc->allocate_node(node_element, "use_widestring");
        use_widestring_text_node->value(doc->allocate_string(std::to_string(label->UseWidestringText).c_str()));
        view_node->append_node(use_widestring_text_node);

        xml_node<>* color_node = doc->allocate_node(node_element, "color");
        color_node->append_attribute(doc->allocate_attribute("r", doc->allocate_string(std::to_string(label->color.r).c_str())));
        color_node->append_attribute(doc->allocate_attribute("g", doc->allocate_string(std::to_string(label->color.g).c_str())));
        color_node->append_attribute(doc->allocate_attribute("b", doc->allocate_string(std::to_string(label->color.b).c_str())));
        color_node->append_attribute(doc->allocate_attribute("a", doc->allocate_string(std::to_string(label->color.alpha).c_str())));
        view_node->append_node(color_node);

        xml_node<>* text_properties_node = doc->allocate_node(node_element, "text_properties");

        xml_node<>* font_node = doc->allocate_node(node_element, "font");
        font_node->value(doc->allocate_string(label->Properties.Font.c_str()));
        text_properties_node->append_node(font_node);

        xml_node<>* font_size_node = doc->allocate_node(node_element, "font_size");
        font_size_node->value(doc->allocate_string(std::to_string(label->Properties.FontSize).c_str()));
        text_properties_node->append_node(font_size_node);

        const auto alignment_to_string = [](TextAlignment alignment) -> std::string {
            switch (alignment)
            {
            case TextAlignment::CENTERED: return "mc::TextAlignment::CENTERED";
            case TextAlignment::LEADING: return "mc::TextAlignment::LEADING";
            case TextAlignment::TRAILING: return "mc::TextAlignment::TRAILING";
            default: return "Unknown";
            }
        };

        const auto wrapping_to_string = [](WordWrapping wrapping) -> std::string {
            switch (wrapping)
            {
            case WordWrapping::CHARACTER_WRAP: return "mc::WordWrapping::CHARACTER_WRAP";
            case WordWrapping::NORMAL_WRAP: return "mc::WordWrapping::NORMAL_WRAP";
            case WordWrapping::NO_WRAP: return "mc::WordWrapping::NO_WRAP";
            case WordWrapping::EMERGENCY_BREAK: return "mc::WordWrapping::EMERGENCY_BREAK";
            case WordWrapping::WORD_WRAP: return "mc::WordWrapping::WORD_WRAP";
            default: return "Unknown";
            }
        };

        const auto style_to_string = [](TextStyle style) -> std::string {
            switch (style)
            {
            case TextStyle::DEFAULT: return "mc::TextStyle::DEFAULT";
            case TextStyle::BOLD: return "mc::TextStyle::BOLD";
            case TextStyle::BOLD_ITALIC: return "mc::TextStyle::BOLD_ITALIC";
            case TextStyle::ITALIC: return "mc::TextStyle::ITALIC";
            case TextStyle::SEMIBOLD_ITALIC: return "mc::TextStyle::SEMIBOLD_ITALIC";
            case TextStyle::SEMIBOLD: return "mc::TextStyle::SEMIBOLD";
            case TextStyle::Light: return "mc::TextStyle::Light";
            default: return "Unknown";
            }
        };

        xml_node<>* alignment_node = doc->allocate_node(node_element, "alignment");
        alignment_node->value(doc->allocate_string(alignment_to_string(label->Properties.Alignment).c_str()));
        text_properties_node->append_node(alignment_node);

        xml_node<>* style_node = doc->allocate_node(node_element, "style");
        style_node->value(doc->allocate_string(style_to_string(label->Properties.Style).c_str()));
        text_properties_node->append_node(style_node);

        xml_node<>* wrapping_node = doc->allocate_node(node_element, "wrapping");
        wrapping_node->value(doc->allocate_string(wrapping_to_string(label->Properties.Wrapping).c_str()));
        text_properties_node->append_node(wrapping_node);

        view_node->append_node(text_properties_node);
    }

    void AddButtonPropertyNodes(Ref<xml_document<>>& doc, xml_node<>*& view_node, Ref<UIButton>& button)
    {
        xml_node<>* filled_node = doc->allocate_node(node_element, "filled");
        filled_node->value(doc->allocate_string(std::to_string(button->Filled).c_str()));
        view_node->append_node(filled_node);

        xml_node<>* stroke_node = doc->allocate_node(node_element, "stroke");
        stroke_node->value(doc->allocate_string(std::to_string(button->Filled).c_str()));
        view_node->append_node(stroke_node);

        xml_node<>* hover_on_node = doc->allocate_node(node_element, "hover_on_color");
        hover_on_node->append_attribute(doc->allocate_attribute("r", doc->allocate_string(std::to_string(button->HoverOnColor.r).c_str())));
        hover_on_node->append_attribute(doc->allocate_attribute("g", doc->allocate_string(std::to_string(button->HoverOnColor.g).c_str())));
        hover_on_node->append_attribute(doc->allocate_attribute("b", doc->allocate_string(std::to_string(button->HoverOnColor.b).c_str())));
        hover_on_node->append_attribute(doc->allocate_attribute("a", doc->allocate_string(std::to_string(button->HoverOnColor.alpha).c_str())));
        view_node->append_node(hover_on_node);

        xml_node<>* on_mouse_press_color = doc->allocate_node(node_element, "on_mouse_press_color");
        on_mouse_press_color->append_attribute(doc->allocate_attribute("r", doc->allocate_string(std::to_string(button->HoverOnColor.r).c_str())));
        on_mouse_press_color->append_attribute(doc->allocate_attribute("g", doc->allocate_string(std::to_string(button->HoverOnColor.g).c_str())));
        on_mouse_press_color->append_attribute(doc->allocate_attribute("b", doc->allocate_string(std::to_string(button->HoverOnColor.b).c_str())));
        on_mouse_press_color->append_attribute(doc->allocate_attribute("a", doc->allocate_string(std::to_string(button->HoverOnColor.alpha).c_str())));
        view_node->append_node(on_mouse_press_color);
    }

    void AddCheckboxPropertyNodes(Ref<xml_document<>>& doc, xml_node<>*& view_node, Ref<UICheckbox>& checkbox)
    {
        xml_node<>* checked_node = doc->allocate_node(node_element, "checked");
        checked_node->value(doc->allocate_string(std::to_string(checkbox->Checked).c_str()));
        view_node->append_node(checked_node);

        xml_node<>* box_size_node = doc->allocate_node(node_element, "box_size");
        box_size_node->value(doc->allocate_string(std::to_string(checkbox->BoxSize).c_str()));
        view_node->append_node(box_size_node);

        xml_node<>* label_margins_node = doc->allocate_node(node_element, "label_margins_size");
        label_margins_node->value(doc->allocate_string(std::to_string(checkbox->BoxSize).c_str()));
        view_node->append_node(label_margins_node);

        xml_node<>* checkmark_color = doc->allocate_node(node_element, "checkmark_color");
        checkmark_color->append_attribute(doc->allocate_attribute("r", doc->allocate_string(std::to_string(checkbox->CheckmarkColor.r).c_str())));
        checkmark_color->append_attribute(doc->allocate_attribute("g", doc->allocate_string(std::to_string(checkbox->CheckmarkColor.g).c_str())));
        checkmark_color->append_attribute(doc->allocate_attribute("b", doc->allocate_string(std::to_string(checkbox->CheckmarkColor.b).c_str())));
        checkmark_color->append_attribute(doc->allocate_attribute("a", doc->allocate_string(std::to_string(checkbox->CheckmarkColor.alpha).c_str())));
        view_node->append_node(checkmark_color);

        xml_node<>* checkedbox_color = doc->allocate_node(node_element, "checkedbox_color");
        checkedbox_color->append_attribute(doc->allocate_attribute("r", doc->allocate_string(std::to_string(checkbox->CheckedBoxColor.r).c_str())));
        checkedbox_color->append_attribute(doc->allocate_attribute("g", doc->allocate_string(std::to_string(checkbox->CheckedBoxColor.g).c_str())));
        checkedbox_color->append_attribute(doc->allocate_attribute("b", doc->allocate_string(std::to_string(checkbox->CheckedBoxColor.b).c_str())));
        checkedbox_color->append_attribute(doc->allocate_attribute("a", doc->allocate_string(std::to_string(checkbox->CheckedBoxColor.alpha).c_str())));
        view_node->append_node(checkedbox_color);
    }

    void AddSliderPropertyNodes(Ref<xml_document<>>& doc, xml_node<>*& view_node, Ref<UISlider>& slider)
    {
        std::string knob_shape_string = (slider->SliderKnobShape == Shape::Rectangle) ? "rectangle" : "circle";
        xml_node<>* knob_shape_node = doc->allocate_node(node_element, "knob_shape");
        knob_shape_node->value(doc->allocate_string(knob_shape_string.c_str()));
        view_node->append_node(knob_shape_node);

        xml_node<>* slider_bar_height_node = doc->allocate_node(node_element, "sliderbar_height");
        slider_bar_height_node->value(doc->allocate_string(std::to_string(slider->SliderBarHeight).c_str()));
        view_node->append_node(slider_bar_height_node);

        xml_node<>* max_val_node = doc->allocate_node(node_element, "max_value");
        max_val_node->value(doc->allocate_string(std::to_string(slider->MaxValue).c_str()));
        view_node->append_node(max_val_node);

        xml_node<>* min_val_node = doc->allocate_node(node_element, "min_value");
        min_val_node->value(doc->allocate_string(std::to_string(slider->MinValue).c_str()));
        view_node->append_node(min_val_node);

        xml_node<>* val_node = doc->allocate_node(node_element, "value");
        val_node->value(doc->allocate_string(std::to_string(slider->Value).c_str()));
        view_node->append_node(val_node);

        xml_node<>* intervals_node = doc->allocate_node(node_element, "intervals");
        intervals_node->value(doc->allocate_string(std::to_string(slider->Intervals).c_str()));
        view_node->append_node(intervals_node);

        xml_node<>* tickmark_visibility_node = doc->allocate_node(node_element, "visible_tickmarks");
        tickmark_visibility_node->value(doc->allocate_string(std::to_string(slider->VisibleTickmarks).c_str()));
        view_node->append_node(tickmark_visibility_node);

        xml_node<>* knob_color_node = doc->allocate_node(node_element, "knob_color");
        knob_color_node->append_attribute(doc->allocate_attribute("r", doc->allocate_string(std::to_string(slider->SliderKnobColor.r).c_str())));
        knob_color_node->append_attribute(doc->allocate_attribute("g", doc->allocate_string(std::to_string(slider->SliderKnobColor.g).c_str())));
        knob_color_node->append_attribute(doc->allocate_attribute("b", doc->allocate_string(std::to_string(slider->SliderKnobColor.b).c_str())));
        knob_color_node->append_attribute(doc->allocate_attribute("a", doc->allocate_string(std::to_string(slider->SliderKnobColor.alpha).c_str())));
        view_node->append_node(knob_color_node);

        xml_node<>* tickmarks_color_node = doc->allocate_node(node_element, "tickmarks_color");
        tickmarks_color_node->append_attribute(doc->allocate_attribute("r", doc->allocate_string(std::to_string(slider->SliderKnobColor.r).c_str())));
        tickmarks_color_node->append_attribute(doc->allocate_attribute("g", doc->allocate_string(std::to_string(slider->SliderKnobColor.g).c_str())));
        tickmarks_color_node->append_attribute(doc->allocate_attribute("b", doc->allocate_string(std::to_string(slider->SliderKnobColor.b).c_str())));
        tickmarks_color_node->append_attribute(doc->allocate_attribute("a", doc->allocate_string(std::to_string(slider->SliderKnobColor.alpha).c_str())));
        view_node->append_node(tickmarks_color_node);
    }

    void AddWindowSettingsNode(Ref<xml_document<>>& doc, xml_node<>*& root_node, WindowSettings& window_settings)
    {
        xml_node<>* width_node = doc->allocate_node(node_element, "width");
        width_node->value(doc->allocate_string(std::to_string(window_settings.width).c_str()));

        xml_node<>* height_node = doc->allocate_node(node_element, "height");
        height_node->value(doc->allocate_string(std::to_string(window_settings.height).c_str()));

        xml_node<>* title_node = doc->allocate_node(node_element, "title");
        title_node->value(doc->allocate_string(window_settings.title.c_str()));

        xml_node<>* color_node = doc->allocate_node(node_element, "color");
        color_node->append_attribute(doc->allocate_attribute("r", doc->allocate_string(std::to_string(window_settings.color.r).c_str())));
        color_node->append_attribute(doc->allocate_attribute("g", doc->allocate_string(std::to_string(window_settings.color.g).c_str())));
        color_node->append_attribute(doc->allocate_attribute("b", doc->allocate_string(std::to_string(window_settings.color.b).c_str())));
        color_node->append_attribute(doc->allocate_attribute("a", doc->allocate_string(std::to_string(window_settings.color.alpha).c_str())));

        xml_node<>* window_node = doc->allocate_node(node_element, "mcwindow");
        window_node->append_node(width_node);
        window_node->append_node(height_node);
        window_node->append_node(title_node);
        window_node->append_node(color_node);

        root_node->append_node(window_node);
    }

    //============================================================================================================================================//
    //============================================================================================================================================//
    //============================================================================================================================================//
    //============================================================================================================================================//
    //============================================================================================================================================//

    void ReadWindowSettingsNode(xml_node<>* node, MCLayout& layout)
    {
        // node is equal to mcwindow

        // Read values from xml document
        const char* windowWidth = node->first_node("width")->value();
        const char* windowHeight = node->first_node("height")->value();
        const char* windowTitle = node->first_node("title")->value();
        const char* colorR = node->first_node("color")->first_attribute("r")->value();
        const char* colorG = node->first_node("color")->first_attribute("g")->value();
        const char* colorB = node->first_node("color")->first_attribute("b")->value();
        const char* colorA = node->first_node("color")->first_attribute("a")->value();

        // Convert color values into one string
        std::string color = ConnectColorStrings(colorR, colorG, colorB, colorA);

        // Save the values in the layout
        layout.windowSettings.width = StringToUInt(windowWidth);
        layout.windowSettings.height = StringToUInt(windowHeight);
        layout.windowSettings.color = StringToColor(color);
        layout.windowSettings.title = windowTitle;
    }

    Ref<UIView> ReadUILabelNode(xml_node<>* node)
    {
        Ref<UILabel> label = MakeRef<UILabel>();
        xml_node<>* frameNode = node->first_node("layer")->first_node("frame");
        xml_node<>* frameColorNode = node->first_node("layer")->first_node("color");
        xml_node<>* textPropsNode = node->first_node("text_properties");

        float frameSizeWidth = StringToFloat(frameNode->first_node("size")->first_node("width")->value());
        float frameSizeHeight = StringToFloat(frameNode->first_node("size")->first_node("height")->value());
        float framePosX = StringToFloat(frameNode->first_node("position")->first_node("x")->value());
        float framePosY = StringToFloat(frameNode->first_node("position")->first_node("y")->value());

        std::string frameColorR = frameColorNode->first_attribute("r")->value();
        std::string frameColorG = frameColorNode->first_attribute("g")->value();
        std::string frameColorB = frameColorNode->first_attribute("b")->value();
        std::string frameColorA = frameColorNode->first_attribute("a")->value();
        Color frameColor = StringToColor(ConnectColorStrings(frameColorR, frameColorG, frameColorB, frameColorA));

        uint32_t zIndex = StringToUInt(node->first_node("z_index")->value());
        bool visible = StringToBool(node->first_node("visible")->value());
        bool useWideString = StringToBool(node->first_node("use_widestring")->value());
        float cornerRadius = StringToFloat(node->first_node("corner_radius")->value());
        std::string labelText = node->first_node("text")->value();

        std::string labelColorR = node->first_node("color")->first_attribute("r")->value();
        std::string labelColorG = node->first_node("color")->first_attribute("g")->value();
        std::string labelColorB = node->first_node("color")->first_attribute("b")->value();
        std::string labelColorA = node->first_node("color")->first_attribute("a")->value();
        Color labelColor = StringToColor(ConnectColorStrings(labelColorR, labelColorG, labelColorB, labelColorA));

        std::string fontName = textPropsNode->first_node("font")->value();
        uint32_t fontSize = StringToUInt(textPropsNode->first_node("font_size")->value());
        TextAlignment alignment = StringToTextPropertiesAlignment(textPropsNode->first_node("alignment")->value());
        TextStyle style = StringToTextPropertiesStyle(textPropsNode->first_node("style")->value());
        WordWrapping wrapping = StringToTextPropertiesWrapping(textPropsNode->first_node("wrapping")->value());

        label->color = labelColor;
        label->CornerRadius = cornerRadius;
        label->layer.color = frameColor;
        label->layer.frame.position.x = framePosX;
        label->layer.frame.position.y = framePosY;
        label->layer.frame.size.width = frameSizeWidth;
        label->layer.frame.size.height = frameSizeHeight;
        label->SetZIndex(zIndex);
        label->Properties.Alignment = alignment;
        label->Properties.Font = fontName;
        label->Properties.FontSize = fontSize;
        label->Properties.Style = style;
        label->Properties.Wrapping = wrapping;
        label->UseWidestringText = useWideString;
        label->Visible = visible;

        if (useWideString)
            label->WidestringText = std::wstring(labelText.begin(), labelText.end());
        else
            label->Text = labelText;

        return label;
    }

    Ref<UIView> ReadUIButtonNode(xml_node<>* node)
    {
        Ref<UIButton> button = MakeRef<UIButton>();
        xml_node<>* frameNode = node->first_node("layer")->first_node("frame");
        xml_node<>* frameColorNode = node->first_node("layer")->first_node("color");
        xml_node<>* textLayerNode = node->first_node("uiview");
        xml_node<>* textFrameNode = textLayerNode->first_node("layer")->first_node("frame");
        xml_node<>* textFrameColorNode = textLayerNode->first_node("layer")->first_node("color");
        xml_node<>* textProps = textLayerNode->first_node("text_properties");

        // Load Button Properties
        float buttonWidth = StringToFloat(frameNode->first_node("size")->first_node("width")->value());
        float buttonHeight = StringToFloat(frameNode->first_node("size")->first_node("height")->value());
        float buttonXPos = StringToFloat(frameNode->first_node("position")->first_node("x")->value());
        float buttonYPos = StringToFloat(frameNode->first_node("position")->first_node("y")->value());

        std::string buttonColorR = frameColorNode->first_attribute("r")->value();
        std::string buttonColorG = frameColorNode->first_attribute("g")->value();
        std::string buttonColorB = frameColorNode->first_attribute("b")->value();
        std::string buttonColorA = frameColorNode->first_attribute("a")->value();
        Color buttonColor = StringToColor(ConnectColorStrings(buttonColorR, buttonColorG, buttonColorB, buttonColorA));

        std::string buttonHoverColorR = node->first_node("hover_on_color")->first_attribute("r")->value();
        std::string buttonHoverColorG = node->first_node("hover_on_color")->first_attribute("g")->value();
        std::string buttonHoverColorB = node->first_node("hover_on_color")->first_attribute("b")->value();
        std::string buttonHoverColorA = node->first_node("hover_on_color")->first_attribute("a")->value();
        Color buttonHoverColor = StringToColor(ConnectColorStrings(buttonHoverColorR, buttonHoverColorG, buttonHoverColorB, buttonHoverColorA));

        std::string buttonPressedColorR = node->first_node("on_mouse_press_color")->first_attribute("r")->value();
        std::string buttonPressedColorG = node->first_node("on_mouse_press_color")->first_attribute("g")->value();
        std::string buttonPressedColorB = node->first_node("on_mouse_press_color")->first_attribute("b")->value();
        std::string buttonPressedColorA = node->first_node("on_mouse_press_color")->first_attribute("a")->value();
        Color buttonPressedColor = StringToColor(ConnectColorStrings(buttonPressedColorR, buttonPressedColorG, buttonPressedColorB, buttonPressedColorA));

        float buttonCornerRadius = StringToFloat(node->first_node("corner_radius")->value());
        float buttonStroke = StringToFloat(node->first_node("stroke")->value());
        uint32_t buttonZIndex = StringToUInt(node->first_node("z_index")->value());
        bool buttonVisibility = StringToBool(node->first_node("visible")->value());
        bool buttonIsFilled = StringToBool(node->first_node("filled")->value());

        // Load Label Properties
        float labelWidth = StringToFloat(textFrameNode->first_node("size")->first_node("width")->value());
        float labelHeight = StringToFloat(textFrameNode->first_node("size")->first_node("height")->value());
        float labelXPos = StringToFloat(textFrameNode->first_node("position")->first_node("x")->value());
        float labelYPos = StringToFloat(textFrameNode->first_node("position")->first_node("y")->value());

        std::string labelFrameColorR = textFrameColorNode->first_attribute("r")->value();
        std::string labelFrameColorG = textFrameColorNode->first_attribute("g")->value();
        std::string labelFrameColorB = textFrameColorNode->first_attribute("b")->value();
        std::string labelFrameColorA = textFrameColorNode->first_attribute("a")->value();
        Color labelFrameColor = StringToColor(ConnectColorStrings(labelFrameColorR, labelFrameColorG, labelFrameColorB, labelFrameColorA));

        uint32_t labelZIndex = StringToUInt(textLayerNode->first_node("z_index")->value());
        bool labelVisibility = StringToBool(textLayerNode->first_node("visible")->value());
        bool labelUseWideString = StringToBool(textLayerNode->first_node("use_widestring")->value());
        float labelCornerRadius = StringToFloat(textLayerNode->first_node("corner_radius")->value());
        std::string labelText = textLayerNode->first_node("text")->value();

        std::string labelColorR = textLayerNode->first_node("color")->first_attribute("r")->value();
        std::string labelColorG = textLayerNode->first_node("color")->first_attribute("g")->value();
        std::string labelColorB = textLayerNode->first_node("color")->first_attribute("b")->value();
        std::string labelColorA = textLayerNode->first_node("color")->first_attribute("a")->value();
        Color labelColor = StringToColor(ConnectColorStrings(labelColorR, labelColorG, labelColorB, labelColorA));

        std::string labelFontName = textProps->first_node("font")->value();
        uint32_t labelFontSize = StringToUInt(textProps->first_node("font_size")->value());
        TextAlignment labelAlignment = StringToTextPropertiesAlignment(textProps->first_node("alignment")->value());
        TextStyle labelStyle = StringToTextPropertiesStyle(textProps->first_node("style")->value());
        WordWrapping labelWrapping = StringToTextPropertiesWrapping(textProps->first_node("wrapping")->value());

        // Set Button Properties
        button->SetZIndex(buttonZIndex);
        button->Filled = buttonIsFilled;
        button->Stroke = buttonStroke;
        button->CornerRadius = buttonCornerRadius;
        button->HoverOnColor = buttonHoverColor;
        button->OnMousePressColor = buttonPressedColor;
        button->Visible = buttonVisibility;
        button->layer.frame.size.width = buttonWidth;
        button->layer.frame.size.height = buttonHeight;
        button->layer.frame.position.x = buttonXPos;
        button->layer.frame.position.y = buttonYPos;
        button->layer.color = buttonColor;

        // Set Label Properties
        button->Label->SetZIndex(labelZIndex);
        button->Label->CornerRadius = labelCornerRadius;
        button->Label->color = labelColor;
        button->Label->layer.frame.size.width = labelWidth;
        button->Label->layer.frame.size.height = labelHeight;
        button->Label->layer.frame.position.x = labelXPos;
        button->Label->layer.frame.position.y = labelYPos;
        button->Label->layer.color = labelFrameColor;
        button->Label->UseWidestringText = labelUseWideString;
        button->Label->Visible = labelVisibility;
        button->Label->Properties.Font = labelFontName;
        button->Label->Properties.FontSize = labelFontSize;
        button->Label->Properties.Alignment = labelAlignment;
        button->Label->Properties.Style = labelStyle;
        button->Label->Properties.Wrapping = labelWrapping;

        if (labelUseWideString)
            button->Label->WidestringText = std::wstring(labelText.begin(), labelText.end());
        else
            button->Label->Text = labelText;

        return button;
    }

    Ref<UIView> ReadUIViewNode(xml_node<>* node)
    {
        Ref<UIView> view = MakeRef<UIView>();
        xml_node<>* frameNode = node->first_node("layer")->first_node("frame");
        xml_node<>* frameColorNode = node->first_node("layer")->first_node("color");

        float width = StringToFloat(frameNode->first_node("size")->first_node("width")->value());
        float height = StringToFloat(frameNode->first_node("size")->first_node("height")->value());
        float xPos = StringToFloat(frameNode->first_node("position")->first_node("x")->value());
        float yPos = StringToFloat(frameNode->first_node("position")->first_node("y")->value());

        std::string colorR = frameColorNode->first_attribute("r")->value();
        std::string colorG = frameColorNode->first_attribute("g")->value();
        std::string colorB = frameColorNode->first_attribute("b")->value();
        std::string colorA = frameColorNode->first_attribute("a")->value();
        Color color = StringToColor(ConnectColorStrings(colorR, colorG, colorB, colorA));

        uint32_t zIndex = StringToUInt(node->first_node("z_index")->value());
        float cornerRadius = StringToFloat(node->first_node("corner_radius")->value());
        bool visible = StringToBool(node->first_node("visible")->value());

        view->layer.frame.size.width = width;
        view->layer.frame.size.height = height;
        view->layer.frame.position.x = xPos;
        view->layer.frame.position.y = yPos;
        view->layer.color = color;
        view->SetZIndex(zIndex);
        view->CornerRadius = cornerRadius;
        view->Visible = visible;

        return view;
    }

    Ref<UIView> ReadUISliderNode(xml_node<>* node)
    {
        Ref<UISlider> slider = MakeRef<UISlider>();
        xml_node<>* frameNode = node->first_node("layer")->first_node("frame");
        xml_node<>* frameColorNode = node->first_node("layer")->first_node("color");

        float sliderWidth = StringToFloat(frameNode->first_node("size")->first_node("width")->value());
        float sliderHeight = StringToFloat(frameNode->first_node("size")->first_node("height")->value());
        float sliderXPos = StringToFloat(frameNode->first_node("position")->first_node("x")->value());
        float sliderYPos = StringToFloat(frameNode->first_node("position")->first_node("y")->value());

        std::string frameColorR = frameColorNode->first_attribute("r")->value();
        std::string frameColorG = frameColorNode->first_attribute("g")->value();
        std::string frameColorB = frameColorNode->first_attribute("b")->value();
        std::string frameColorA = frameColorNode->first_attribute("a")->value();
        Color frameColor = StringToColor(ConnectColorStrings(frameColorR, frameColorG, frameColorB, frameColorA));

        uint32_t zIndex = StringToUInt(node->first_node("z_index")->value());
        float cornerRadius = StringToFloat(node->first_node("corner_radius")->value());
        float maxValue = StringToFloat(node->first_node("max_value")->value());
        float minValue = StringToFloat(node->first_node("min_value")->value());
        float value = StringToFloat(node->first_node("value")->value());
        float intervals = StringToFloat(node->first_node("intervals")->value());
        float barHeight = StringToFloat(node->first_node("sliderbar_height")->value());
        bool visibleTickmarks = StringToBool(node->first_node("visible_tickmarks")->value());
        bool visible = StringToBool(node->first_node("visible")->value());
        Shape knobShape = StringToShape(node->first_node("knob_shape")->value());

        std::string knobColorR = node->first_node("knob_color")->first_attribute("r")->value();
        std::string knobColorG = node->first_node("knob_color")->first_attribute("g")->value();
        std::string knobColorB = node->first_node("knob_color")->first_attribute("b")->value();
        std::string knobColorA = node->first_node("knob_color")->first_attribute("a")->value();
        Color knobColor = StringToColor(ConnectColorStrings(knobColorR, knobColorG, knobColorB, knobColorA));

        std::string tickmarksColorR = node->first_node("tickmarks_color")->first_attribute("r")->value();
        std::string tickmarksColorG = node->first_node("tickmarks_color")->first_attribute("g")->value();
        std::string tickmarksColorB = node->first_node("tickmarks_color")->first_attribute("b")->value();
        std::string tickmarksColorA = node->first_node("tickmarks_color")->first_attribute("a")->value();
        Color tickmarksColor = StringToColor(ConnectColorStrings(tickmarksColorR, tickmarksColorG, tickmarksColorB, tickmarksColorA));

        slider->SetZIndex(zIndex);
        slider->CornerRadius = cornerRadius;
        slider->Intervals = intervals;
        slider->MaxValue = maxValue;
        slider->MinValue = minValue;
        slider->Value = value;
        slider->SliderBarHeight = barHeight;
        slider->SliderKnobColor = knobColor;
        slider->SliderKnobShape = knobShape;
        slider->TickmarksColor = tickmarksColor;
        slider->Visible = visible;
        slider->VisibleTickmarks = visibleTickmarks;
        slider->layer.color = frameColor;
        slider->layer.frame.position.x = sliderXPos;
        slider->layer.frame.position.y = sliderYPos;
        slider->layer.frame.size.width = sliderWidth;
        slider->layer.frame.size.height = sliderHeight;

        return slider;
    }

    Ref<UIView> ReadUICheckboxNode(xml_node<>* node)
    {
        Ref<UICheckbox> checkbox = MakeRef<UICheckbox>();
        xml_node<>* frameNode = node->first_node("layer")->first_node("frame");
        xml_node<>* frameColorNode = node->first_node("layer")->first_node("color");
        xml_node<>* labelNode = node->first_node("uiview");
        xml_node<>* labelFrameNode = node->first_node("uiview")->first_node("layer")->first_node("frame");
        xml_node<>* labelFrameColorNode = node->first_node("uiview")->first_node("layer")->first_node("color");
        xml_node<>* labelTextPropsNode = node->first_node("uiview")->first_node("text_properties");

        // Read checkbox Properties
        float checkboxWidth = StringToFloat(frameNode->first_node("size")->first_node("width")->value());
        float checkboxHeight = StringToFloat(frameNode->first_node("size")->first_node("height")->value());
        float checkboxXPos = StringToFloat(frameNode->first_node("position")->first_node("x")->value());
        float checkboxYPos = StringToFloat(frameNode->first_node("position")->first_node("y")->value());

        std::string frameColorR = frameColorNode->first_attribute("r")->value();
        std::string frameColorG = frameColorNode->first_attribute("g")->value();
        std::string frameColorB = frameColorNode->first_attribute("b")->value();
        std::string frameColorA = frameColorNode->first_attribute("a")->value();
        Color frameColor = StringToColor(ConnectColorStrings(frameColorR, frameColorG, frameColorB, frameColorA));

        uint32_t zIndex = StringToUInt(node->first_node("z_index")->value());
        bool visible = StringToBool(node->first_node("visible")->value());
        bool checked = StringToBool(node->first_node("checked")->value());
        float cornerRadius = StringToFloat(node->first_node("corner_radius")->value());
        float boxSize = StringToFloat(node->first_node("box_size")->value());
        float labelMarginSize = StringToFloat(node->first_node("label_margins_size")->value());

        std::string checkmarkColorR = node->first_node("checkmark_color")->first_attribute("r")->value();
        std::string checkmarkColorG = node->first_node("checkmark_color")->first_attribute("g")->value();
        std::string checkmarkColorB = node->first_node("checkmark_color")->first_attribute("b")->value();
        std::string checkmarkColorA = node->first_node("checkmark_color")->first_attribute("a")->value();
        Color checkmarkColor = StringToColor(ConnectColorStrings(checkmarkColorR, checkmarkColorG, checkmarkColorB, checkmarkColorA));

        std::string checkedboxColorR = node->first_node("checkedbox_color")->first_attribute("r")->value();
        std::string checkedboxColorG = node->first_node("checkedbox_color")->first_attribute("g")->value();
        std::string checkedboxColorB = node->first_node("checkedbox_color")->first_attribute("b")->value();
        std::string checkedboxColorA = node->first_node("checkedbox_color")->first_attribute("a")->value();
        Color checkedboxColor = StringToColor(ConnectColorStrings(checkedboxColorR, checkedboxColorG, checkedboxColorB, checkedboxColorA));

        // Read label Properties
        float labelWidth = StringToFloat(labelFrameNode->first_node("size")->first_node("width")->value());
        float labelHeight = StringToFloat(labelFrameNode->first_node("size")->first_node("height")->value());
        float labelXPos = StringToFloat(labelFrameNode->first_node("position")->first_node("x")->value());
        float labelYPos = StringToFloat(labelFrameNode->first_node("position")->first_node("y")->value());

        std::string labelFrameColorR = labelFrameColorNode->first_attribute("r")->value();
        std::string labelFrameColorG = labelFrameColorNode->first_attribute("g")->value();
        std::string labelFrameColorB = labelFrameColorNode->first_attribute("b")->value();
        std::string labelFrameColorA = labelFrameColorNode->first_attribute("a")->value();
        Color labelFrameColor = StringToColor(ConnectColorStrings(labelFrameColorR, labelFrameColorG, labelFrameColorB, labelFrameColorA));

        uint32_t labelZIndex = StringToUInt(labelNode->first_node("z_index")->value());
        float labelCornerRadius = StringToFloat(labelNode->first_node("corner_radius")->value());
        bool labelVisibility = StringToBool(labelNode->first_node("visible")->value());
        bool labelUseWideString = StringToBool(labelNode->first_node("use_widestring")->value());
        std::string labelText = labelNode->first_node("text")->value();

        std::string labelColorR = labelNode->first_node("color")->first_attribute("r")->value();
        std::string labelColorG = labelNode->first_node("color")->first_attribute("g")->value();
        std::string labelColorB = labelNode->first_node("color")->first_attribute("b")->value();
        std::string labelColorA = labelNode->first_node("color")->first_attribute("a")->value();
        Color labelColor = StringToColor(ConnectColorStrings(labelColorR, labelColorG, labelColorB, labelColorA));

        std::string labelFontName = labelTextPropsNode->first_node("font")->value();
        uint32_t labelFontSize = StringToUInt(labelTextPropsNode->first_node("font_size")->value());
        TextAlignment labelAlignment = StringToTextPropertiesAlignment(labelTextPropsNode->first_node("alignment")->value());
        TextStyle labelStyle = StringToTextPropertiesStyle(labelTextPropsNode->first_node("style")->value());
        WordWrapping labelWrapping = StringToTextPropertiesWrapping(labelTextPropsNode->first_node("wrapping")->value());

        // Set checkbox Properties
        checkbox->BoxSize = boxSize;
        checkbox->Checked = checked;
        checkbox->CheckedBoxColor = checkedboxColor;
        checkbox->CheckmarkColor = checkmarkColor;
        checkbox->CornerRadius = cornerRadius;
        checkbox->LabelMargins = labelMarginSize;
        checkbox->layer.color = frameColor;
        checkbox->layer.frame.position.x = checkboxXPos;
        checkbox->layer.frame.position.y = checkboxYPos;
        checkbox->layer.frame.size.width = checkboxWidth;
        checkbox->layer.frame.size.height = checkboxHeight;
        checkbox->Visible = visible;
        checkbox->SetZIndex(zIndex);

        // Set label Properties
        checkbox->Label->color = labelColor;
        checkbox->Label->layer.color = labelFrameColor;
        checkbox->Label->layer.frame.position.x = labelXPos;
        checkbox->Label->layer.frame.position.y = labelYPos;
        checkbox->Label->layer.frame.size.width = labelWidth;
        checkbox->Label->layer.frame.size.height = labelHeight;
        checkbox->Label->CornerRadius = labelCornerRadius;
        checkbox->Label->UseWidestringText = labelUseWideString;
        checkbox->Label->Visible = labelVisibility;
        checkbox->Label->Properties.Font = labelFontName;
        checkbox->Label->Properties.FontSize = labelFontSize;
        checkbox->Label->Properties.Alignment = labelAlignment;
        checkbox->Label->Properties.Style = labelStyle;
        checkbox->Label->Properties.Wrapping = labelWrapping;
        checkbox->Label->SetZIndex(labelZIndex);

        return checkbox;
    }

    void CheckAndResolveViewNodeType(xml_node<>* node, Ref<UIView>* outValue)
    {
        std::string type = node->first_attribute("type")->value();

        if (type == "UIView")
            *outValue = ReadUIViewNode(node);
        else if (type == "UILabel")
            *outValue = ReadUILabelNode(node);
        else if (type == "UIButton")
            *outValue = ReadUIButtonNode(node);
        else if (type == "UICheckbox")
            *outValue = ReadUICheckboxNode(node);
        else if (type == "UISlider")
            *outValue = ReadUISliderNode(node);
    }

    void ReadUINodes(xml_node<>* node, MCLayout& layout)
    {
        std::vector<Ref<UIView>> views;

        while (node != 0)
        {
            Ref<UIView> element = nullptr;
            CheckAndResolveViewNodeType(node, &element);

            if (element)
                views.push_back(element);

            node = node->next_sibling("uiview");
        }

        layout.uiViews = views;
    }

    MCLayout ProjectGenerator::LoadMCProject(const std::string& path)
    {
        MCLayout layout;

        std::ifstream file(path);
        Ref<xml_document<>> document = MakeRef<xml_document<>>();

        std::vector<char> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        buffer.push_back('\0');
        document->parse<0>(&buffer[0]);

        xml_node<>* root_node = document->first_node();
        ReadWindowSettingsNode(root_node->first_node("mcwindow"), layout);
        ReadUINodes(root_node->first_node("uiview"), layout);

        file.close();
        return layout;
    }
}
