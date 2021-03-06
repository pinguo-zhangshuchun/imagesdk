LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_WHOLE_STATIC_LIBRARIES := libpng libz libjpeg
LOCAL_MODULE 	:= imgsdk

LOCAL_SRC_FILES := imgsdk.c \
				   android_main.c

# Must enable when BUILD_EXECUTABLE
# And disable when BUILD_SHARED_LIBRARY
#LOCAL_CFLAGS	+= -pie -fPIE

LOCAL_LDLIBS	:= -llog -lGLESv2 -lEGL -landroid

# Must enable when BUILD_EXECUTABLE
# And disable when BUILD_SHARED_LIBRARY
#LOCAL_LDFLAGS   += -pie -fPIE

LOCAL_STATIC_LIBRARIES := android_native_app_glue

include $(BUILD_SHARED_LIBRARY)
#include $(BUILD_EXECUTABLE)

$(call import-module, android/native_app_glue)
