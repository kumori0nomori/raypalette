#include "ui/preview.hpp"

#include <cstdint>
#include <string>

#include "imgui.h"

namespace raypalette::ui {

void draw_preview(PreviewContext& context) {
  ImGui::SetNextWindowSize(ImVec2(context.layout.width, context.layout.height),
                           ImGuiCond_FirstUseEver);
  ImGui::Begin("Preview");
  if (context.texture.id != 0) {
    ImGui::Image(
        reinterpret_cast<ImTextureID>(static_cast<std::intptr_t>(context.texture.id)),
        ImVec2(static_cast<float>(context.image.width), static_cast<float>(context.image.height)),
        ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));

    const ImVec2 image_min = ImGui::GetItemRectMin();
    const ImVec2 image_max = ImGui::GetItemRectMax();
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      const ImVec2 mouse = ImGui::GetIO().MousePos;
      const float u = (mouse.x - image_min.x) / (image_max.x - image_min.x);
      const float v = (mouse.y - image_min.y) / (image_max.y - image_min.y);
      const int pixel_x = static_cast<int>(u * context.image.width);
      const int pixel_y = context.image.height - 1 - static_cast<int>(v * context.image.height);
      if (pixel_x >= 0 && pixel_x < context.image.width && pixel_y >= 0 &&
          pixel_y < context.image.height) {
        const Vec3 picked_color =
            context.texture.display_pixels[pixel_y * context.image.width + pixel_x];
        if (context.gui_state.palette.size() < kMaximumPaletteColors) {
          bool already_added = false;
          for (const PaletteColor& entry : context.gui_state.palette) {
            already_added = already_added || same_palette_color(entry.color, picked_color);
          }
          if (!already_added) {
            context.gui_state.palette.push_back({picked_color, color_to_hex(picked_color), u, v});
            context.gui_state.selected_palette_index =
                static_cast<int>(context.gui_state.palette.size()) - 1;
          }
        }
      }
    }

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    for (int index = 0; index < static_cast<int>(context.gui_state.palette.size()); ++index) {
      const PaletteColor& entry = context.gui_state.palette[index];
      const ImVec2 pin_position(image_min.x + entry.u * (image_max.x - image_min.x),
                                image_min.y + entry.v * (image_max.y - image_min.y));
      const bool selected = context.gui_state.selected_palette_index == index;
      const ImU32 pin_color = ImGui::GetColorU32(selected ? ImGuiCol_PlotHistogram : ImGuiCol_Text);
      draw_list->AddCircleFilled(pin_position, selected ? 6.0f : 5.0f, pin_color);
      draw_list->AddCircle(pin_position, selected ? 9.0f : 8.0f,
                           ImGui::GetColorU32(ImGuiCol_WindowBg), 16, 2.0f);
      draw_list->AddText(ImVec2(pin_position.x + 8.0f, pin_position.y - 8.0f), pin_color,
                         std::to_string(index).c_str());
    }
  }
  if (ImGui::BeginTabBar("PreviewTabs")) {
    if (ImGui::BeginTabItem("Palette")) {
      if (ImGui::Button("Clear palette")) {
        context.gui_state.palette.clear();
        context.gui_state.selected_palette_index = -1;
      }
      for (int index = 0; index < static_cast<int>(context.gui_state.palette.size()); ++index) {
        ImGui::PushID(index);
        const PaletteColor& entry = context.gui_state.palette[index];
        ImGui::Text("%d", index);
        ImGui::SameLine();
        const ImVec4 swatch(entry.color.x, entry.color.y, entry.color.z, 1.0f);
        ImGui::ColorButton("##swatch", swatch, ImGuiColorEditFlags_NoTooltip, ImVec2(32.0f, 24.0f));
        ImGui::SameLine();
        if (ImGui::Selectable(entry.hex.c_str(), context.gui_state.selected_palette_index == index,
                              ImGuiSelectableFlags_AllowDoubleClick)) {
          context.gui_state.selected_palette_index = index;
        }
        if (context.gui_state.selected_palette_index == index && ImGui::GetIO().KeyCtrl &&
            ImGui::IsKeyPressed(ImGuiKey_C)) {
          ImGui::SetClipboardText(entry.hex.c_str());
        }
        ImGui::PopID();
      }
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
  ImGui::End();
}

} // namespace raypalette::ui