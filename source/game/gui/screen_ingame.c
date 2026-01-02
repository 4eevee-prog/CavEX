//*
	Copyright (c) 2022 ByteBit/xtreme8000

	This file is part of CavEX.

	CavEX is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	CavEX is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with CavEX.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stddef.h>
#include <malloc.h>

#include "../../block/blocks.h"
#include "../../graphics/gfx_util.h"
#include "../../graphics/gui_util.h"
#include "../../graphics/render_model.h"
#include "../../network/server_interface.h"
#include "../../particle.h"
#include "../../platform/gfx.h"
#include "../../platform/input.h"
#include "../../platform/audio.h"
#include "../game_state.h"

/* ------------------------------------------------------------ */

static void screen_ingame_reset(struct screen* s, int width, int height) {
	input_pointer_enable(false);

	if(gstate.local_player)
		gstate.local_player->data.local_player.capture_input = true;
}

/* ------------------------------------------------------------ */

void screen_ingame_render3D(struct screen* s, mat4 view) {
	if(gstate.world_loaded && gstate.camera_hit.hit) {
		struct block_data blk
			= world_get_block(&gstate.world, gstate.camera_hit.x,
							  gstate.camera_hit.y, gstate.camera_hit.z);

		if(gstate.digging.active)
			render_block_cracks(&blk, view, gstate.camera_hit.x,
								gstate.camera_hit.y, gstate.camera_hit.z);

		gfx_blending(MODE_BLEND);
		gfx_alpha_test(false);

		gutil_block_selection(view,
							  &(struct block_info) {
								  .block = &blk,
								  .x = gstate.camera_hit.x,
								  .y = gstate.camera_hit.y,
								  .z = gstate.camera_hit.z,
							  });

		gfx_blending(MODE_OFF);
		gfx_alpha_test(true);
	}

	float place_lerp = 0.0F;
	size_t slot = inventory_get_hotbar(
		windowc_get_latest(gstate.windows[WINDOWC_INVENTORY]));

	float dig_lerp
		= time_diff_s(gstate.held_item_animation.punch.start, time_get())
		/ 0.4F;

	if(gstate.held_item_animation.punch.place)
		place_lerp = 1.0F - glm_clamp(dig_lerp * 4.0F, 0, 1);

	if(dig_lerp >= 1.0F)
		dig_lerp = 0.0F;

	float swing_lerp
		= time_diff_s(gstate.held_item_animation.switch_item.start, time_get())
		/ 0.3F;

	if(swing_lerp < 0.5F)
		slot = gstate.held_item_animation.switch_item.old_slot;

	if(swing_lerp >= 1.0F)
		swing_lerp = 0.0F;

	float sinHalfCircle = sinf(dig_lerp * GLM_PI);
	float sqrtLerpPI = sqrtf(dig_lerp) * GLM_PI;
	float sinHalfCircleWeird = sinf(glm_pow2(dig_lerp) * GLM_PI);

	struct block_data in_block
		= world_get_block(&gstate.world, floorf(gstate.camera.x),
						  floorf(gstate.camera.y), floorf(gstate.camera.z));
	uint8_t light = (in_block.torch_light << 4) | in_block.sky_light;

	gfx_depth_range(0.0F, 0.1F);

	mat4 model;
	struct item_data item;

	if(inventory_get_slot(windowc_get_latest(gstate.windows[WINDOWC_INVENTORY]),
						  slot + INVENTORY_SLOT_HOTBAR, &item)
	   && item_get(&item)) {

		glm_translate_make(model,
						   (vec3) {0.56F - sinf(sqrtLerpPI) * 0.4F,
								   -0.52F + sinf(sqrtLerpPI * 2.0F) * 0.2F
									   - 0.6F * place_lerp
									   - 0.4F * sinf(swing_lerp * GLM_PI),
								   -0.72F - sinHalfCircle * 0.2F});
		glm_rotate_y(model, glm_rad(45.0F), model);
		glm_rotate_y(model, glm_rad(-sinHalfCircleWeird * 20.0F), model);
		glm_rotate_z(model, glm_rad(-sinf(sqrtLerpPI) * 20.0F), model);
		glm_rotate_x(model, glm_rad(-sinf(sqrtLerpPI) * 80.0F), model);

		glm_scale_uni(model, 0.4F);
		glm_translate(model, (vec3) {-0.5F, -0.5F, -0.5F});
		render_item_update_light(light);
		items[item.id]->renderItem(item_get(&item), &item, model, false,
								   R_ITEM_ENV_FIRSTPERSON);
	}

	gfx_depth_range(0.0F, 1.0F);
}

/* ------------------------------------------------------------ */
/*                    UPDATE (LOGICA + AUDIO)                   */
/* ------------------------------------------------------------ */

static void screen_ingame_update(struct screen* s, float dt) {

	/* --- PLACE BLOCK --- */
	if(gstate.camera_hit.hit && input_pressed(IB_ACTION2)
	   && !gstate.digging.active) {

		svin_rpc_send(&(struct server_rpc) {
			.type = SRPC_BLOCK_PLACE,
			.payload.block_place.x = gstate.camera_hit.x,
			.payload.block_place.y = gstate.camera_hit.y,
			.payload.block_place.z = gstate.camera_hit.z,
			.payload.block_place.side = gstate.camera_hit.side,
		});

		if(inventory_get_hotbar_item(
			   windowc_get_latest(gstate.windows[WINDOWC_INVENTORY]), NULL)) {

			gstate.held_item_animation.punch.start = time_get();
			gstate.held_item_animation.punch.place = true;

			audio_play_sfx(SFX_PLACE);
		}
	}

	/* --- DIGGING --- */
	if(gstate.digging.active) {
		struct block_data blk
			= world_get_block(&gstate.world, gstate.digging.x,
							  gstate.digging.y, gstate.digging.z);

		struct item_data it;
		inventory_get_hotbar_item(
			windowc_get_latest(gstate.windows[WINDOWC_INVENTORY]), &it);

		int delay = blocks[blk.type]
			? tool_dig_delay_ms(blocks[blk.type], item_get(&it))
			: 0;

		if(delay > 0
		   && time_diff_ms(gstate.digging.start, time_get()) >= delay) {

			svin_rpc_send(&(struct server_rpc) {
				.type = SRPC_BLOCK_DIG,
				.payload.block_dig.x = gstate.digging.x,
				.payload.block_dig.y = gstate.digging.y,
				.payload.block_dig.z = gstate.digging.z,
				.payload.block_dig.side = gstate.camera_hit.side,
				.payload.block_dig.finished = true,
			});

			audio_play_sfx(SFX_BREAK);

			gstate.digging.cooldown = time_get();
			gstate.digging.active = false;
		}

		if(input_released(IB_ACTION1))
			gstate.digging.active = false;
	}

	/* --- HOTBAR SCROLL --- */
	size_t slot = inventory_get_hotbar(
		windowc_get_latest(gstate.windows[WINDOWC_INVENTORY]));

	if(input_pressed(IB_SCROLL_LEFT)) {
		size_t next_slot = (slot == 0) ? INVENTORY_SIZE_HOTBAR - 1 : slot - 1;
		inventory_set_hotbar(
			windowc_get_latest(gstate.windows[WINDOWC_INVENTORY]), next_slot);

		audio_play_sfx(SFX_ITEM_SWITCH);

		svin_rpc_send(&(struct server_rpc) {
			.type = SRPC_HOTBAR_SLOT,
			.payload.hotbar_slot.slot = next_slot,
		});
	}

	if(input_pressed(IB_SCROLL_RIGHT)) {
		size_t next_slot = (slot == INVENTORY_SIZE_HOTBAR - 1) ? 0 : slot + 1;
		inventory_set_hotbar(
			windowc_get_latest(gstate.windows[WINDOWC_INVENTORY]), next_slot);

		audio_play_sfx(SFX_ITEM_SWITCH);

		svin_rpc_send(&(struct server_rpc) {
			.type = SRPC_HOTBAR_SLOT,
			.payload.hotbar_slot.slot = next_slot,
		});
	}

	/* --- INVENTORY --- */
	if(input_pressed(IB_INVENTORY)) {
		audio_play_sfx(SFX_GUI_CLICK);
		screen_set(&screen_inventory);
	}

	/* --- EXIT WORLD --- */
	if(input_pressed(IB_HOME)) {
		audio_play_sfx(SFX_GUI_CLICK);
		screen_set(&screen_select_world);

		svin_rpc_send(&(struct server_rpc) {
			.type = SRPC_UNLOAD_WORLD,
		});
	}
}

/* ------------------------------------------------------------ */

static void screen_ingame_render2D(struct screen* s, int width, int height) {
	/* SIN CAMBIOS */
}

/* ------------------------------------------------------------ */

struct screen screen_ingame = {
	.reset = screen_ingame_reset,
	.update = screen_ingame_update,
	.render2D = screen_ingame_render2D,
	.render3D = screen_ingame_render3D,
	.render_world = true,
};
