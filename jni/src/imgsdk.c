/***************************
 * file name:   imgsdk.c
 * description:
 * author:      kari.zhang
 * date:        2015-05-09
 *
 ***************************/

#include <malloc.h>
#include <string.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <png.h>
#include "imgsdk.h"

typedef struct {
    GLchar  *cmd;
    GLuint  cmdLen;
    GLuint  cmdSize;
    GLuint  paramCount;
} UserCmd;


typedef void (*CallbackFunc)(SdkEnv *);

typedef struct SdkEnv {

    //struct android_app* app; // native Activity

    // EGL
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    EGLint    eglWidth;
    EGLint    eglHeight;
    EGLNativeWindowType window;
    
    // openGL ES2
    GLuint programHandle;
    GLuint positionHandle;

    // user data
    GLuint width;
    GLuint height;
    GLchar *path;
    GLchar *pixel;

    // user cmd
    UserCmd* cmd;

    // callback	
    CallbackFunc onDraw;
    CallbackFunc onRender;
} SdkEnv;

typedef struct UserData {
    // handle to a pragram object
    GLuint program;

    // attribute location;
    GLint  positionLoc;
    GLint  textureLoc;

    // uniform location
    GLint matrixModeLoc;
    GLint martrixViewLoc;
    GLint matrixPerspectiveLoc;

    // sampler location
    GLint samplerLoc;

    // texture
    GLint texture;
} UserData;

int main(int argc, char **argv) {
    if (2 != argc) {
        Log("Usage:\n");
        Log("  %s path\n", argv[0]);
        return -1;
    }

    Bitmap_t img;
    memset(&img, 0, sizeof(img));
    if(read_png(argv[1], &img) < 0) {
        LogE("Failed read png\n");
        return -1;
    }

    SdkEnv *env = defaultSdkEnv();
    if (NULL == env) {
        LogE("Failed get SdkEnv instance\n");
    }

	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1024, 1024, 0, GL_RGBA, GL_UNSIGNED_INT, img.base); 
	if(!glIsTexture(tex)) {
		LogE("Failed gen texture\n");
	}

    GLuint fb;
    glGenFramebuffers(1, &fb);
	glBindFramebuffer(GL_FRAMEBUFFER, fb);
    if (!glIsFramebuffer(fb)) {
        LogE("Failed glGenFramebuffer\n");
    }
	
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex,0);

	GLuint rb_color;
	glGenRenderbuffers(1, &rb_color);
	glBindRenderbuffer(GL_RENDERBUFFER, rb_color);
	if (!glIsRenderbuffer(rb_color)) {
		LogE("Failed genRenderbuffer\n");
	}
	glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA, img.width, img.height);

	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb_color);

	//eglWaitGL();
    //eglWaitClient();
	eglWaitNative(EGL_CORE_NATIVE_ENGINE);


	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (GL_FRAMEBUFFER_COMPLETE != status) {
		LogE("framebuffer status:0x%X\n", status);
	}
	
    // copy pixels from GPU memory to CPU memory
    glReadPixels(0, 0, img.width, img.height, GL_RGBA, GL_UNSIGNED_INT, img.base);

    if (write_png("2.png", &img) < 0) {
        LogE("Failed write png\n");
    }

    freeBitmap(&img);
    freeSdkEnv(env);
}

int read_png(const char *path, Bitmap_t *mem)
{
    VALIDATE_NOT_NULL2(path, mem);
    FILE *fp = fopen(path, "rb");
    if (NULL == fp) {
        return FILE_NOT_EXIST;
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 
            0, 0, 0);
    png_infop info_ptr = png_create_info_struct(png_ptr);
    setjmp(png_jmpbuf(png_ptr));
    png_init_io(png_ptr, fp);
    png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_EXPAND, 0);
    int width = png_get_image_width(png_ptr, info_ptr);
    int height = png_get_image_height(png_ptr,info_ptr);
    int type = png_get_color_type(png_ptr, info_ptr);
    png_bytep *row_pointers = png_get_rows(png_ptr, info_ptr);

    int bpp = 0;
    switch (type) { 
        case PNG_COLOR_TYPE_GRAY:
            bpp = 1;
            break;

        case PNG_COLOR_TYPE_RGB:
            bpp = 3;
            break;

        case PNG_COLOR_TYPE_RGB_ALPHA:
            bpp = 4;
            break;
    }

    if (bpp == 0) {
        LogE("Failed get pixel format");
    }

    mem->form = bpp;
    mem->width = width;
    mem->height = height;

    Log("[%s %d x %d bpp=%d]\n", path, width, height, bpp);

    int size = width * height * bpp;
    if (mem->base == NULL) {
        mem->base = (char *)calloc(size, 1);
        if (mem->base == NULL) {
            LogE("Failed calloc mem\n");
        }
    }

    char *start = mem->base;
    int i, j;
    for (i = 0; i < height; ++i) {
        memcpy(start, row_pointers[i], width * bpp);
        start += width * bpp;
    }
    png_destroy_read_struct(&png_ptr, &info_ptr, 0);
    fclose(fp);
    return size;
}

int write_png(const char *path, Bitmap_t *mem)
{
    VALIDATE_NOT_NULL2(path, mem);
    FILE *fp;
    png_structp png_ptr;
    png_infop info_ptr;
    png_colorp palette;

    fp = fopen(path, "wb");
    if (NULL == fp) {
        LogE("Failed open file");
        return -1;
    }

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (NULL == png_ptr) {
        fclose (fp);
        return -1;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (NULL == info_ptr) {
        fclose (fp);
        png_destroy_write_struct(&png_ptr, NULL);
        return -1;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        LogE("write file occur error");
        fclose(fp);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        return -1;
    }

    int form = PNG_COLOR_TYPE_RGB_ALPHA;
    int depth = 8;
    switch (mem->form) {
        case RGBA32:
            form = PNG_COLOR_TYPE_RGBA;
            break;

        case RGB24:
            form = PNG_COLOR_TYPE_RGB;
            break;

        case GRAY:
            form = PNG_COLOR_TYPE_GRAY;
            depth = 1;
            break;
    }

    png_init_io(png_ptr, fp);
    png_set_IHDR(png_ptr, info_ptr, mem->width, mem->height, depth, form, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    png_write_info(png_ptr, info_ptr);
    png_bytep row_pointers[mem->height];
    int k;
    for (k = 0; k < mem->height; ++k) {
        row_pointers[k] = mem->base + k * mem->width * mem->form;
    }
    png_write_image(png_ptr, row_pointers);
    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose (fp);

    return 0;
}

int readFile(const char* path, char** mem)
{
    VALIDATE_NOT_NULL2(path, mem);
    FILE *fp = fopen(path, "r");
    if (NULL == fp) {
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (NULL == *mem) {
        (*mem) = (char *)calloc(1, size + 1);
        if (NULL == *mem) {
            LogE("Failed calloc mem for shader file\n");
            close (fp);
            return -1;
        }
    }

    if (fread(*mem, 1, size, fp) != size) {
        LogE("Failed read shader to mem\n");
        close(fp);
        free (*mem);
        *mem = NULL;
        return -1;
    }

    return 0;
}

#define LOG_BUFF_SIZE 1024
static char gLogBuff[LOG_BUFF_SIZE];

static int loadShader(GLenum type, const char* source) {
    VALIDATE_NOT_NULL(source);
    GLuint shader = glCreateShader(type);
    if (!shader) {
        LogE("Invalid shader\n");
        return 0;
    }
    GLint length = strlen(source);
    glShaderSource(shader, 1, &source, &length);
    glCompileShader(shader);
    int compiled, len;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
		memset(gLogBuff, 0, LOG_BUFF_SIZE);
        glGetShaderInfoLog(shader, LOG_BUFF_SIZE, &len, gLogBuff);
        if (len > 0) {
            Log("compile error:%s\n", gLogBuff);
        } else {
            LogE("Failed get compile log\n");
        }
        shader = 0;
        glDeleteShader(shader);
	}

	return shader;
}

int createProgram(const char* vertexSource, const char* fragSource)
{
    VALIDATE_NOT_NULL2(vertexSource, fragSource);
    int program = 0;
    int vertShader = 0;
    int fragShader = 0;

    do {
        vertShader = loadShader(GL_VERTEX_SHADER, vertexSource);
        if (!vertShader) {
            LogE("Failed load vertex shader\n");
            break;
        }

        fragShader = loadShader(GL_FRAGMENT_SHADER, fragSource);
        if (!fragShader) {
            LogE("Failed load fragment sahder\n");
            break;
        }

        // Mark to delete, fre automaticlly when do not use any longer
        //glDeleteShader(vertShader);
        //glDeleteShader(fragShader);

        program = glCreateProgram();
        if (!program) {
            LogE("Failed create program\n");
            break;
        }

        glAttachShader(program, vertShader);
        glAttachShader(program, fragShader);
        glLinkProgram(program);
        GLint linkStatus = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        GLint len;
        if (!linkStatus) {
			memset(gLogBuff, 0, LOG_BUFF_SIZE);
            glGetProgramInfoLog(program, LOG_BUFF_SIZE, &len, gLogBuff); 
            if (len > 0) {
                Log("link error log:%s\n", gLogBuff);
            } else {
                LogE("Failed get link log\n");
            }
            break;
        }

		glUseProgram(program);

        return program;

    } while (0);

    if (!program) {
        glDeleteProgram(program);
    }
    if (!vertShader) {
        glDeleteShader(vertShader);
    }
    if (!fragShader) {
        glDeleteShader(fragShader);
    }
    
    return 0;
}

void freeBitmap(Bitmap_t *mem)
{
    if (NULL != mem) {
        if (NULL != mem->base) {
            free (mem->base);
            mem->base = NULL;
        }
    }
}

static int initEGL(SdkEnv *env)
{
    VALIDATE_NOT_NULL(env);
    env->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (EGL_NO_DISPLAY == env->display || EGL_SUCCESS != eglGetError()) {
        LogE("Failed eglGetDisplay\n");
        return -1;
    }
    EGLint major, minor;
    if (!eglInitialize(env->display, &major, &minor) || EGL_SUCCESS != eglGetError()) {
        LogE("Failed eglInitialize\n");
        return -1;
    }
    Log("EGL %d.%d\n", major, minor);

    EGLConfig configs[2];
    EGLint numConfigs;
    if (eglGetConfigs(env->display, configs, 2, &numConfigs) == EGL_FALSE || EGL_SUCCESS != eglGetError()) {
        LogE("Failed eglGetConfigs\n");
        return -1;
    }

    EGLint cfg_attrs[] = { 
        EGL_SURFACE_TYPE,    EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_BLUE_SIZE,       8,
        EGL_GREEN_SIZE,      8,
        EGL_RED_SIZE,        8,
        EGL_NONE
    };
	if (!eglChooseConfig(env->display, cfg_attrs, configs, 2, &numConfigs) || numConfigs < 1) {
		LogE("Failed eglChooseConfig\n");
		return -1;
	}

	EGLint surface_attrs[] = {
		EGL_WIDTH, SURFACE_MAX_WIDTH,   // pBuffer needs at least 1 x 1
		EGL_HEIGHT,SURFACE_MAX_HEIGHT,  // pBuffer needs at least 1 x 1 
		EGL_TEXTURE_TARGET, EGL_NO_TEXTURE,
		EGL_TEXTURE_FORMAT, EGL_NO_TEXTURE,
		EGL_NONE
	};
    env->surface = eglCreatePbufferSurface(env->display, configs[0], surface_attrs);

    if (EGL_NO_SURFACE == env->surface) {
        LogE("Failed create Pbuffer Surface, error code:%x\n", eglGetError());
        return -1;
    }

    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        LogE("Failed eglBindAPI\n");
        return -1;
    }

    EGLint context_attrs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    env->context = eglCreateContext(env->display, configs[0], EGL_NO_CONTEXT, context_attrs);
    if (EGL_NO_CONTEXT == env->context) {
        LogE("Failed create context\n");
        Log("error code:%x\n", eglGetError());
        return -1;
    }

    if (!eglMakeCurrent(env->display, env->surface, env->surface, env->context)) {
        LogE("Failed eglMakeCurrent\n");
        EGLint errcode = eglGetError();
        LogE("error code:%x\n", errcode);
        return -1;
    }

    eglQuerySurface(env->display, env->surface, EGL_WIDTH, &env->eglWidth);
    eglQuerySurface(env->display, env->surface, EGL_HEIGHT, &env->eglHeight);
    Log("Max support %d x %d\n", env->eglWidth, env->eglHeight);

    return 0;
}

int freeSdkEnv(SdkEnv *env)
{
    if (NULL == env) {
        return -1;
    }

    if (env->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(env->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (env->context != EGL_NO_CONTEXT) {
            eglDestroyContext(env->display, env->context);
        }
        if (env->surface != EGL_NO_SURFACE) {
            eglDestroySurface(env->display, env->surface);
        }
        eglTerminate(env->display);
    }

    env->display = EGL_NO_DISPLAY;
    env->context = EGL_NO_CONTEXT;
    env->surface = EGL_NO_SURFACE;
    free (env);
}

SdkEnv* defaultSdkEnv() {
    SdkEnv *env = (SdkEnv *)calloc(1, sizeof(SdkEnv));
    if (NULL == env){
        return NULL;
    }
    if (initEGL(env) < 0) {
        LogE("Failed initEGL\n");
        freeSdkEnv(env);
        return NULL;
    }

	// log openGL information
    const GLubyte* version = glGetString(GL_VERSION);
    Log("%s\n", version);

    char *vertSource = NULL;
    int count = readFile(VERT_SHADER_FILE, &vertSource);
    if (count < 0) {
        LogE("Failed open vertex shader file:%s\n", VERT_SHADER_FILE);
        freeSdkEnv(env);
        return NULL;
    }
    char *fragSource = NULL;
    count = readFile(FRAG_SHADER_FILE, &fragSource);
    if (count < 0) {
        LogE("Failed read fragment shader file:%s\n", FRAG_SHADER_FILE);
        freeSdkEnv(env);
        free (vertSource);
        return NULL;
    }

    GLint program = createProgram(vertSource, fragSource);
    if (!program) {
        LogE("Failed createProgram\n");
        freeSdkEnv(env);
        free (vertSource);
        free (fragSource);
        return NULL;
    }

    env->programHandle = program;

    return env;
}

int setEffectCmd(SdkEnv* env, const char* cmd)
{
    VALIDATE_NOT_NULL2(env, cmd);

    return -1;
}

int parseEffectCmd(SdkEnv* env)
{
    VALIDATE_NOT_NULL(env);
    
    return -1;
}

