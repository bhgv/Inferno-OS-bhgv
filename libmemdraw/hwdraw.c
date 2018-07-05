#include "lib9.h"
#include "draw.h"
#include "memdraw.h"


#ifdef ANDROID

#include <android/log.h>

#define  LOG_TAG    "inferno HW"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGW(...)  __android_log_print(ANDROID_LOG_WARN,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

#else

#define  LOG_TAG    "inferno HW"
#define  LOGI(...)  //__android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGW(...)  //__android_log_print(ANDROID_LOG_WARN,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  //__android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

#endif


int
hwdraw(Memdrawparam *par)
{
	return 0;	/* could not satisfy request */
}

