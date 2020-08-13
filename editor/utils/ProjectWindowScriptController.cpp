#include "ProjectWindowScriptController.h"

void ProjectWindowScriptController::Widget_OnMousePressed(Ref<UIView> view)
{
	m_MouseButtonReleasedFromView = false;
	m_TargetView = view;
	m_ViewClickedLocationOffset = view->srcwindow->GetMouseCursorPos() - view->GetAbsolutePosition();

	std::thread dragging_thread(&ProjectWindowScriptController::DragView, this);
	dragging_thread.detach();
}

void ProjectWindowScriptController::Widget_OnMouseReleased()
{
	m_MouseButtonReleasedFromView = true;

	if (m_TargetView)
	{
		CheckEmbeddingStatus();
	}
}

void ProjectWindowScriptController::FindInnerMostView(Ref<UIView> view, Ref<UIView>& target)
{
	if (!view) return;

	for (auto& child : view->subviews)
	{
		if (!IsViewEmbeddable(view.get())) continue;

		Frame frame = child->layer.frame;
		frame.position = child->GetAbsolutePosition();

		if (frame.DoesContain(view->srcwindow->GetMouseCursorPos()))
		{
			target = child;
			FindInnerMostView(child, target);
		}
	}
}

void ProjectWindowScriptController::DragView()
{
	while (!m_MouseButtonReleasedFromView)
	{
		if (m_TargetView->srcwindow)
		{
			m_TargetView->layer.frame.position = m_TargetView->srcwindow->GetMouseCursorPos() - m_ViewClickedLocationOffset;
			UIView* parent = m_TargetView->parent;
			while (parent)
			{
				m_TargetView->layer.frame.position -= parent->layer.frame.position;
				parent = parent->parent;
			}
		}

		Sleep(2);
	}
}

void ProjectWindowScriptController::CheckEmbeddingStatus()
{
	bool should_unembed = true;
	while (should_unembed)
	{
		should_unembed = UnembedViewIfNeeded(m_TargetView);
	}

	Ref<UIView> embeddable = nullptr;
	auto* elements = m_ProjectUIElements;
	while (CheckIfViewNeedsEmbedding(m_TargetView, *elements, embeddable))
		elements = &embeddable->subviews;

	if (embeddable && embeddable.get() != m_TargetView->parent)
	{
		if (m_TargetView->parent) m_TargetView->parent->RemoveSubview(m_TargetView);
		EmbedView(m_TargetView, embeddable);
	}
}

bool ProjectWindowScriptController::IsViewEmbeddable(UIView* view)
{
	if (utils::CheckType<UILabel>(view)) return false;
	if (utils::CheckType<UIButton>(view)) return false;
	if (utils::CheckType<UICheckbox>(view)) return false;
	if (utils::CheckType<UISlider>(view)) return false;
	if (utils::CheckType<UITextbox>(view)) return false;
	if (utils::CheckType<UICombobox>(view)) return false;

	return true;
}

bool ProjectWindowScriptController::CheckIfViewNeedsEmbedding(Ref<UIView>& view_being_checked, std::vector<Ref<UIView>>& elements, Ref<UIView>& embeddable)
{
	for (auto& view : elements)
	{
		if (view.get() == view_being_checked.get()) continue;

		Frame frame = view->layer.frame;
		frame.position = view->GetAbsolutePosition();

		if (frame.DoesContain(view->srcwindow->GetMouseCursorPos()))
		{
			embeddable = view;
			return true;
		}
	}

	return false;
}

void ProjectWindowScriptController::EmbedView(Ref<UIView> src, Ref<UIView> dest)
{
	auto it = std::find(m_ProjectUIElements->begin(), m_ProjectUIElements->end(), src);
	if (it != m_ProjectUIElements->end())
	{
		m_ProjectUIElements->erase(it);
		(*m_ProjectWindow)->RemoveView(src);
	}

	auto position_offset = dest->srcwindow->GetMouseCursorPos() - dest->GetAbsolutePosition();
	src->layer.frame.position = position_offset - m_ViewClickedLocationOffset;

	dest->AddSubview(src);
}

bool ProjectWindowScriptController::UnembedViewIfNeeded(Ref<UIView> view)
{
	bool result = false;

	// If the view inside another view, check if it needs to be taken out of it.
	if (view->parent)
	{
		Frame frame = view->parent->layer.frame;
		frame.position = view->parent->GetAbsolutePosition();

		if (!frame.DoesContain(view->srcwindow->GetMouseCursorPos()))
		{
			UIView* parent_view = view->parent;
			view->layer.frame.position += parent_view->layer.frame.position;
			parent_view->RemoveSubview(m_TargetView);

			if (parent_view->parent)
			{
				parent_view->parent->AddSubview(view);
			}
			else
			{
				(*m_ProjectWindow)->AddView(view);
				m_ProjectUIElements->push_back(view);
			}

			result = true;
		}

		(*m_ProjectWindow)->ForceUpdate(true);
	}

	return result;
}