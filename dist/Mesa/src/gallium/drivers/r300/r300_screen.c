/*
 * Copyright 2008 Corbin Simpson <MostAwesomeDude@gmail.com>
 * Copyright 2010 Marek Olšák <maraeo@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include "util/u_format.h"
#include "util/u_format_s3tc.h"
#include "util/u_memory.h"

#include "r300_context.h"
#include "r300_texture.h"
#include "r300_screen_buffer.h"
#include "r300_state_inlines.h"
#include "r300_winsys.h"
#include "r300_public.h"

#include "draw/draw_context.h"

#ifdef HAVE_LLVM
#include "gallivm/lp_bld_init.h"
#endif

/* Return the identifier behind whom the brave coders responsible for this
 * amalgamation of code, sweat, and duct tape, routinely obscure their names.
 *
 * ...I should have just put "Corbin Simpson", but I'm not that cool.
 *
 * (Or egotistical. Yet.) */
static const char* r300_get_vendor(struct pipe_screen* pscreen)
{
    return "X.Org R300 Project";
}

static const char* chip_families[] = {
    "ATI R300",
    "ATI R350",
    "ATI R360",
    "ATI RV350",
    "ATI RV370",
    "ATI RV380",
    "ATI R420",
    "ATI R423",
    "ATI R430",
    "ATI R480",
    "ATI R481",
    "ATI RV410",
    "ATI RS400",
    "ATI RC410",
    "ATI RS480",
    "ATI RS482",
    "ATI RS600",
    "ATI RS690",
    "ATI RS740",
    "ATI RV515",
    "ATI R520",
    "ATI RV530",
    "ATI R580",
    "ATI RV560",
    "ATI RV570"
};

static const char* r300_get_name(struct pipe_screen* pscreen)
{
    struct r300_screen* r300screen = r300_screen(pscreen);

    return chip_families[r300screen->caps.family];
}

static int r300_get_param(struct pipe_screen* pscreen, enum pipe_cap param)
{
    struct r300_screen* r300screen = r300_screen(pscreen);
    boolean is_r400 = r300screen->caps.is_r400;
    boolean is_r500 = r300screen->caps.is_r500;

    /* XXX extended shader capabilities of r400 unimplemented */
    is_r400 = FALSE;

    switch (param) {
        /* Supported features (boolean caps). */
        case PIPE_CAP_NPOT_TEXTURES:
        case PIPE_CAP_TWO_SIDED_STENCIL:
        case PIPE_CAP_GLSL:
            /* I'll be frank. This is a lie.
             *
             * We don't truly support GLSL on any of this driver's chipsets.
             * To be fair, no chipset supports the full GLSL specification
             * to the best of our knowledge, but some of the less esoteric
             * features are still missing here.
             *
             * Rather than cripple ourselves intentionally, I'm going to set
             * this flag, and as Gallium's interface continues to change, I
             * hope that this single monolithic GLSL enable can slowly get
             * split down into many different pieces and the state tracker
             * will handle fallbacks transparently, like it should.
             *
             * ~ C.
             */
        case PIPE_CAP_ANISOTROPIC_FILTER:
        case PIPE_CAP_POINT_SPRITE:
        case PIPE_CAP_OCCLUSION_QUERY:
        case PIPE_CAP_TEXTURE_SHADOW_MAP:
        case PIPE_CAP_TEXTURE_MIRROR_CLAMP:
        case PIPE_CAP_TEXTURE_MIRROR_REPEAT:
        case PIPE_CAP_BLEND_EQUATION_SEPARATE:
            return 1;
        case PIPE_CAP_TEXTURE_SWIZZLE:
            return util_format_s3tc_enabled ? r300screen->caps.dxtc_swizzle : 1;

        /* Unsupported features (boolean caps). */
        case PIPE_CAP_TIMER_QUERY:
        case PIPE_CAP_DUAL_SOURCE_BLEND:
        case PIPE_CAP_INDEP_BLEND_ENABLE:
        case PIPE_CAP_INDEP_BLEND_FUNC:
        case PIPE_CAP_DEPTH_CLAMP: /* XXX implemented, but breaks Regnum Online */
        case PIPE_CAP_DEPTHSTENCIL_CLEAR_SEPARATE:
        case PIPE_CAP_SHADER_STENCIL_EXPORT:
        case PIPE_CAP_STREAM_OUTPUT:
        case PIPE_CAP_PRIMITIVE_RESTART:
            return 0;

        /* Texturing. */
        case PIPE_CAP_MAX_TEXTURE_IMAGE_UNITS:
        case PIPE_CAP_MAX_COMBINED_SAMPLERS:
            return r300screen->caps.num_tex_units;
        case PIPE_CAP_MAX_VERTEX_TEXTURE_UNITS:
            return 0;
        case PIPE_CAP_MAX_TEXTURE_2D_LEVELS:
        case PIPE_CAP_MAX_TEXTURE_3D_LEVELS:
        case PIPE_CAP_MAX_TEXTURE_CUBE_LEVELS:
            /* 13 == 4096, 12 == 2048 */
            return is_r500 ? 13 : 12;

        /* Render targets. */
        case PIPE_CAP_MAX_RENDER_TARGETS:
            return 4;

        /* General shader limits and features. */
        case PIPE_CAP_SM3:
            return is_r500 ? 1 : 0;
        /* Fragment coordinate conventions. */
        case PIPE_CAP_TGSI_FS_COORD_ORIGIN_UPPER_LEFT:
        case PIPE_CAP_TGSI_FS_COORD_PIXEL_CENTER_HALF_INTEGER:
	    return 1;
        case PIPE_CAP_TGSI_FS_COORD_ORIGIN_LOWER_LEFT:
        case PIPE_CAP_TGSI_FS_COORD_PIXEL_CENTER_INTEGER:
            return 0;
        default:
            debug_printf("r300: Warning: Unknown CAP %d in get_param.\n",
                         param);
            return 0;
    }
}

static int r300_get_shader_param(struct pipe_screen *pscreen, unsigned shader, enum pipe_shader_cap param)
{
   struct r300_screen* r300screen = r300_screen(pscreen);
   boolean is_r400 = r300screen->caps.is_r400;
   boolean is_r500 = r300screen->caps.is_r500;

   /* XXX extended shader capabilities of r400 unimplemented */
   is_r400 = FALSE;

   switch (shader)
    {
    case PIPE_SHADER_FRAGMENT:
        switch (param)
        {
        case PIPE_SHADER_CAP_MAX_INSTRUCTIONS:
            return is_r500 || is_r400 ? 512 : 96;
        case PIPE_SHADER_CAP_MAX_ALU_INSTRUCTIONS:
            return is_r500 || is_r400 ? 512 : 64;
        case PIPE_SHADER_CAP_MAX_TEX_INSTRUCTIONS:
            return is_r500 || is_r400 ? 512 : 32;
        case PIPE_SHADER_CAP_MAX_TEX_INDIRECTIONS:
            return is_r500 ? 511 : 4;
        case PIPE_SHADER_CAP_MAX_CONTROL_FLOW_DEPTH:
            return is_r500 ? 64 : 0; /* Actually unlimited on r500. */
            /* Fragment shader limits. */
        case PIPE_SHADER_CAP_MAX_INPUTS:
            /* 2 colors + 8 texcoords are always supported
             * (minus fog and wpos).
             *
             * R500 has the ability to turn 3rd and 4th color into
             * additional texcoords but there is no two-sided color
             * selection then. However the facing bit can be used instead. */
            return 10;
        case PIPE_SHADER_CAP_MAX_CONSTS:
            return is_r500 ? 256 : 32;
        case PIPE_SHADER_CAP_MAX_CONST_BUFFERS:
            return 1;
        case PIPE_SHADER_CAP_MAX_TEMPS:
            return is_r500 ? 128 : is_r400 ? 64 : 32;
        case PIPE_SHADER_CAP_MAX_ADDRS:
            return 0;
        case PIPE_SHADER_CAP_MAX_PREDS:
            return is_r500 ? 1 : 0;
        case PIPE_SHADER_CAP_TGSI_CONT_SUPPORTED:
            return 0;
        case PIPE_SHADER_CAP_INDIRECT_INPUT_ADDR:
        case PIPE_SHADER_CAP_INDIRECT_OUTPUT_ADDR:
        case PIPE_SHADER_CAP_INDIRECT_TEMP_ADDR:
        case PIPE_SHADER_CAP_INDIRECT_CONST_ADDR:
            return 0;
        case PIPE_SHADER_CAP_SUBROUTINES:
            return 0;
        }
        break;
    case PIPE_SHADER_VERTEX:
        if (!r300screen->caps.has_tcl) {
            return draw_get_shader_param(shader, param);
        }

        switch (param)
        {
        case PIPE_SHADER_CAP_MAX_INSTRUCTIONS:
        case PIPE_SHADER_CAP_MAX_ALU_INSTRUCTIONS:
            return is_r500 ? 1024 : 256;
        case PIPE_SHADER_CAP_MAX_TEX_INSTRUCTIONS:
        case PIPE_SHADER_CAP_MAX_TEX_INDIRECTIONS:
            return 0;
        case PIPE_SHADER_CAP_MAX_CONTROL_FLOW_DEPTH:
            return is_r500 ? 4 : 0; /* For loops; not sure about conditionals. */
        case PIPE_SHADER_CAP_MAX_INPUTS:
            return 16;
        case PIPE_SHADER_CAP_MAX_CONSTS:
            return 256;
        case PIPE_SHADER_CAP_MAX_CONST_BUFFERS:
            return 1;
        case PIPE_SHADER_CAP_MAX_TEMPS:
            return 32;
        case PIPE_SHADER_CAP_MAX_ADDRS:
            return 1; /* XXX guessed */
        case PIPE_SHADER_CAP_MAX_PREDS:
            return is_r500 ? 4 : 0; /* XXX guessed. */
        case PIPE_SHADER_CAP_TGSI_CONT_SUPPORTED:
            return 0;
        case PIPE_SHADER_CAP_INDIRECT_INPUT_ADDR:
        case PIPE_SHADER_CAP_INDIRECT_OUTPUT_ADDR:
        case PIPE_SHADER_CAP_INDIRECT_TEMP_ADDR:
            return 0;
        case PIPE_SHADER_CAP_INDIRECT_CONST_ADDR:
            return 1;
        case PIPE_SHADER_CAP_SUBROUTINES:
            return 0;
        default:
            break;
        }
        break;
    default:
        break;
    }
    return 0;
}

static float r300_get_paramf(struct pipe_screen* pscreen, enum pipe_cap param)
{
    struct r300_screen* r300screen = r300_screen(pscreen);

    switch (param) {
        case PIPE_CAP_MAX_LINE_WIDTH:
        case PIPE_CAP_MAX_LINE_WIDTH_AA:
        case PIPE_CAP_MAX_POINT_WIDTH:
        case PIPE_CAP_MAX_POINT_WIDTH_AA:
            /* The maximum dimensions of the colorbuffer are our practical
             * rendering limits. 2048 pixels should be enough for anybody. */
            if (r300screen->caps.is_r500) {
                return 4096.0f;
            } else if (r300screen->caps.is_r400) {
                return 4021.0f;
            } else {
                return 2560.0f;
            }
        case PIPE_CAP_MAX_TEXTURE_ANISOTROPY:
            return 16.0f;
        case PIPE_CAP_MAX_TEXTURE_LOD_BIAS:
            return 16.0f;
        case PIPE_CAP_GUARD_BAND_LEFT:
        case PIPE_CAP_GUARD_BAND_TOP:
        case PIPE_CAP_GUARD_BAND_RIGHT:
        case PIPE_CAP_GUARD_BAND_BOTTOM:
            /* XXX I don't know what these should be but the least we can do is
             * silence the potential error message */
            return 0.0f;
        default:
            debug_printf("r300: Warning: Unknown CAP %d in get_paramf.\n",
                         param);
            return 0.0f;
    }
}

static boolean r300_is_format_supported(struct pipe_screen* screen,
                                        enum pipe_format format,
                                        enum pipe_texture_target target,
                                        unsigned sample_count,
                                        unsigned usage,
                                        unsigned geom_flags)
{
    uint32_t retval = 0;
    boolean is_r500 = r300_screen(screen)->caps.is_r500;
    boolean is_r400 = r300_screen(screen)->caps.is_r400;
    boolean is_color2101010 = format == PIPE_FORMAT_R10G10B10A2_UNORM ||
                              format == PIPE_FORMAT_R10G10B10X2_SNORM ||
                              format == PIPE_FORMAT_B10G10R10A2_UNORM ||
                              format == PIPE_FORMAT_R10SG10SB10SA2U_NORM;
    boolean is_ati1n = format == PIPE_FORMAT_RGTC1_UNORM ||
                       format == PIPE_FORMAT_RGTC1_SNORM;
    boolean is_ati2n = format == PIPE_FORMAT_RGTC2_UNORM ||
                       format == PIPE_FORMAT_RGTC2_SNORM;
    boolean is_half_float = format == PIPE_FORMAT_R16_FLOAT ||
                            format == PIPE_FORMAT_R16G16_FLOAT ||
                            format == PIPE_FORMAT_R16G16B16_FLOAT ||
                            format == PIPE_FORMAT_R16G16B16A16_FLOAT;

    /* Check multisampling support. */
    switch (sample_count) {
        case 0:
        case 1:
            break;
        case 2:
        case 3:
        case 4:
        case 6:
            return FALSE;
#if 0
            if (usage != PIPE_BIND_RENDER_TARGET ||
                !util_format_is_rgba8_variant(
                    util_format_description(format))) {
                return FALSE;
            }
#endif
            break;
        default:
            return FALSE;
    }

    /* Check sampler format support. */
    if ((usage & PIPE_BIND_SAMPLER_VIEW) &&
        /* ATI1N is r5xx-only. */
        (is_r500 || !is_ati1n) &&
        /* ATI2N is supported on r4xx-r5xx. */
        (is_r400 || is_r500 || !is_ati2n) &&
        r300_is_sampler_format_supported(format)) {
        retval |= PIPE_BIND_SAMPLER_VIEW;
    }

    /* Check colorbuffer format support. */
    if ((usage & (PIPE_BIND_RENDER_TARGET |
                  PIPE_BIND_DISPLAY_TARGET |
                  PIPE_BIND_SCANOUT |
                  PIPE_BIND_SHARED)) &&
        /* 2101010 cannot be rendered to on non-r5xx. */
        (is_r500 || !is_color2101010) &&
        r300_is_colorbuffer_format_supported(format)) {
        retval |= usage &
            (PIPE_BIND_RENDER_TARGET |
             PIPE_BIND_DISPLAY_TARGET |
             PIPE_BIND_SCANOUT |
             PIPE_BIND_SHARED);
    }

    /* Check depth-stencil format support. */
    if (usage & PIPE_BIND_DEPTH_STENCIL &&
        r300_is_zs_format_supported(format)) {
        retval |= PIPE_BIND_DEPTH_STENCIL;
    }

    /* Check vertex buffer format support. */
    if (usage & PIPE_BIND_VERTEX_BUFFER &&
        /* Half float is supported on >= RV350. */
        (is_r400 || is_r500 || !is_half_float) &&
        r300_translate_vertex_data_type(format) != R300_INVALID_FORMAT) {
        retval |= PIPE_BIND_VERTEX_BUFFER;
    }

    /* Transfers are always supported. */
    if (usage & PIPE_BIND_TRANSFER_READ)
        retval |= PIPE_BIND_TRANSFER_READ;
    if (usage & PIPE_BIND_TRANSFER_WRITE)
        retval |= PIPE_BIND_TRANSFER_WRITE;

    return retval == usage;
}

static void r300_destroy_screen(struct pipe_screen* pscreen)
{
    struct r300_screen* r300screen = r300_screen(pscreen);
    struct r300_winsys_screen *rws = r300_winsys_screen(pscreen);

    util_slab_destroy(&r300screen->pool_buffers);

    if (rws)
      rws->destroy(rws);

    FREE(r300screen);
}

static void r300_fence_reference(struct pipe_screen *screen,
                                 struct pipe_fence_handle **ptr,
                                 struct pipe_fence_handle *fence)
{
    struct r300_fence **oldf = (struct r300_fence**)ptr;
    struct r300_fence *newf = (struct r300_fence*)fence;

    if (pipe_reference(&(*oldf)->reference, &newf->reference))
        FREE(*oldf);

    *ptr = fence;
}

static int r300_fence_signalled(struct pipe_screen *screen,
                                struct pipe_fence_handle *fence,
                                unsigned flags)
{
    struct r300_fence *rfence = (struct r300_fence*)fence;

    return rfence->signalled ? 0 : 1; /* 0 == success */
}

static int r300_fence_finish(struct pipe_screen *screen,
                             struct pipe_fence_handle *fence,
                             unsigned flags)
{
    struct r300_fence *rfence = (struct r300_fence*)fence;

    r300_finish(rfence->ctx);
    rfence->signalled = TRUE;
    return 0; /* 0 == success */
}

struct pipe_screen* r300_screen_create(struct r300_winsys_screen *rws)
{
    struct r300_screen *r300screen = CALLOC_STRUCT(r300_screen);

    if (!r300screen) {
        FREE(r300screen);
        return NULL;
    }

    r300screen->caps.pci_id = rws->get_value(rws, R300_VID_PCI_ID);
    r300screen->caps.num_frag_pipes = rws->get_value(rws, R300_VID_GB_PIPES);
    r300screen->caps.num_z_pipes = rws->get_value(rws, R300_VID_Z_PIPES);

    r300_init_debug(r300screen);
    r300_parse_chipset(&r300screen->caps);

    r300screen->caps.index_bias_supported =
            r300screen->caps.is_r500 &&
            rws->get_value(rws, R300_VID_DRM_2_3_0);

    util_slab_create(&r300screen->pool_buffers,
                     sizeof(struct r300_buffer), 64,
                     UTIL_SLAB_SINGLETHREADED);

    r300screen->rws = rws;
    r300screen->screen.winsys = (struct pipe_winsys*)rws;
    r300screen->screen.destroy = r300_destroy_screen;
    r300screen->screen.get_name = r300_get_name;
    r300screen->screen.get_vendor = r300_get_vendor;
    r300screen->screen.get_param = r300_get_param;
    r300screen->screen.get_shader_param = r300_get_shader_param;
    r300screen->screen.get_paramf = r300_get_paramf;
    r300screen->screen.is_format_supported = r300_is_format_supported;
    r300screen->screen.context_create = r300_create_context;

    r300screen->screen.fence_reference = r300_fence_reference;
    r300screen->screen.fence_signalled = r300_fence_signalled;
    r300screen->screen.fence_finish = r300_fence_finish;

    r300_init_screen_resource_functions(r300screen);

    util_format_s3tc_init();

#ifdef HAVE_LLVM
    lp_build_init();
#endif

    return &r300screen->screen;
}
