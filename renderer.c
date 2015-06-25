#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

#include <gbm.h>
#include <EGL/egl.h>
#include "renderer.h"

static const EGLint RENDER_ATTRIBUTES[] = {
	EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	EGL_RED_SIZE, 1,
	EGL_GREEN_SIZE, 1,
	EGL_BLUE_SIZE, 1,
	EGL_ALPHA_SIZE, 1,
	EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
	EGL_NONE
};

//static EGLint PBUFFER_ATTRIBUTES[] = {
//   EGL_WIDTH,		3200,
//   EGL_HEIGHT,		1800,
//   EGL_NONE
//};

int init_gbm(int fd, render_handler *rh)
{
	rh->gbm = gbm_create_device(fd);
	if (!rh->gbm)
		goto err;
	//info->bo = gbm_bo_create(info->gbm, info->width, info->height,
	//			GBM_BO_FORMAT_XRGB8888,
	//			GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
	//if (!info->bo)
	//	goto err;
	return 0;
err:
	return -errno;
}

static int choose_config(render_handler *rh, EGLConfig *cfg)
{
	int i;
	EGLint count = 0, matched = 0;
	EGLBoolean success;
	EGLConfig *configs;

	success = eglGetConfigs(rh->dpy, NULL, 0, &count);
	if (!success || count < 1)
		goto err_get_config;
	if (!(configs = calloc(count, sizeof(EGLConfig))))
		goto err_get_config;
	success = eglChooseConfig(rh->dpy, RENDER_ATTRIBUTES, configs, count,
			     &matched);
	if (!success || &matched)
		goto err_choose_config;

	//find the suitable config
	for (i = 0; i < count; i++) {
		EGLint gbm_format;

		if (!eglGetConfigAttrib(rh->dpy, configs[i],
					EGL_NATIVE_VISUAL_ID, &gbm_format))
			goto err_choose_config;
		if (gbm_format == GBM_FORMAT_XRGB8888) {
			*cfg = configs[i];
			return 0;
		}
	}
	free(configs);
	return -1;

err_get_config:
	return eglGetError();
err_choose_config:
	free(configs);
	return eglGetError();
}

int init_egl(render_handler *rh, uint32_t width, uint32_t height)
{
	int major, minor;
#ifdef EGL_MESA_platform_gbm
	rh->dpy = eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_MESA, rh->gbm,
					     NULL);
#else
	rh->dpy = eglGetDisplay(rh->gbm);
#endif
	if (rh->dpy == EGL_NO_DISPLAY)
		return -EINVAL;
	eglInitialize(rh->dpy, &major, &minor);
	eglBindAPI(EGL_OPENGL_API);
	if (choose_config(rh, &rh->cfg))
		return -EINVAL;

	rh->native_window = gbm_surface_create(rh->gbm,
						 width, height,
						 GBM_FORMAT_XRGB8888,
						 GBM_BO_USE_SCANOUT |
						 GBM_BO_USE_RENDERING);
	if (!rh->native_window)
		return -1;
#ifdef EGL_MESA_platform_gbm
	rh->sfs = eglCreatePlatformWindowSurfaceEXT(rh->dyp,
						    rh->cfg,
			       (EGLNativeWindowType)rh->native_window,
						    NULL);
#else
	rh->sfs = eglCreateWindowSurface(rh->dpy,
					 rh->cfg,
		    (EGLNativeWindowType)rh->native_window,
					 NULL);
#endif
	if (rh->sfs == EGL_NO_SURFACE) {
		fprintf(stderr, "fail to create surface\n");
		return -1;
	}
	return 0;

}

void destroy_render_handler(render_handler *rh)
{
	if (rh->gbm)
		gbm_device_destroy(rh->gbm);
}
