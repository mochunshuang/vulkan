#pragma once

// types
#include "detail/debug_extension.hpp"
#include "detail/structure_chain.hpp"
#include "detail/instance.hpp"
#include "detail/make_instance.hpp"
#include "detail/physical_device.hpp"
#include "detail/make_physical_device.hpp"

#include "detail/logical_device.hpp"
#include "detail/make_logical_device.hpp"

#include "detail/context_base.hpp"

#include "detail/swap_chain.hpp"
#include "detail/make_swap_chain.hpp"

#include "detail/make_image_base.hpp"
#include "detail/make_deep_image.hpp"
#include "detail/make_color_image.hpp"
#include "detail/make_texture_image.hpp"

#include "detail/choose_swap_min_image_count.hpp"
#include "detail/choose_swap_present_mode.hpp"
#include "detail/choose_swap_surface_format.hpp"
#include "detail/choose_swap_extent.hpp"
#include "detail/context_wsi.hpp"

#include "detail/create_buffer.hpp"

#include "detail/descriptor_resource.hpp"
#include "detail/make_descriptor_resource.hpp"

#include "detail/shader_module.hpp"
#include "detail/copy_buffer.hpp"

// wsi
#include "detail/wsi/glfw.hpp"
