/** ********************************************************************
 * DWM STATUS by <clement@6pi.fr>
 * 
 * Compile with:
 * gcc -Wall -pedantic -std=c99 -lX11 -lasound dwmstatus.c
 * 
 **/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <wchar.h>
#include <unistd.h>
#include <time.h>
#include <X11/Xlib.h>

/* json-c */
#include <stddef.h>
#include <string.h>
#include <json-c/json.h>


/* Alsa */
#include <alsa/asoundlib.h>
#include <alsa/mixer.h>
/* Oss (not working, using popen + ossmix)
#include <linux/soundcard.h>
*/

#define CPU_NBR 9
#define BAR_HEIGHT 20
#define BAT_NOW_FILE "/sys/class/power_supply/BAT0/energy_now"
#define BAT_FULL_FILE "/sys/class/power_supply/BAT0/energy_full"
#define BAT_STATUS_FILE "/sys/class/power_supply/BAT0/status"
#define BRIGHT_NOW_FILE "/sys/class/backlight/intel_backlight/brightness"
#define BRIGHT_FULL_FILE "/sys/class/backlight/intel_backlight/max_brightness"
#define TEMP_SENSOR_FILE "/sys/class/hwmon/hwmon1/temp1_input"
#define FAN_SENSOR_FILE "/sys/class/hwmon/hwmon1/fan1_input"
#define MEMINFO_FILE "/proc/meminfo"
#define BUFFER_FOR_TASK 161 /* length of task name string in characters */

int   getBattery();
int   getBrightness();
int   getBatteryStatus();
int   getMemPercent();
int   getMemFree();
char* getMemColor(int);
void  getCpuUsage(int *cpu_percent);
char* getDateTime();
char* getActiveTask();
float getFreq(char *file);
int   getTemperature();
int   getFan();
int   getVolume();
void  setStatus(Display *dpy, char *str);
int   getWifiPercent();

char* vBar(int percent, int w, int h, char* fg_color, char* bg_color);
char* hBar(int percent, int w, int h, char* fg_color, char* bg_color);
int h2Bar(char *string, size_t size, int percent, int w, int h, char *fg_color, char *bg_color);
int hBarBordered(char *string, size_t size, int percent, int w, int h, char *fg_color, char *bg_color, char *border_color);
int getBatteryBar(char *string, size_t size, int w, int h);
void percentColorGeneric(char* string, int percent, int invert);


/* *******************************************************************
 * MAIN
 ******************************************************************* */

int 
main(void) 
{
  const int MSIZE = 1024;
  Display *dpy;
  char *status;
  
  int cpu_percent[CPU_NBR];
  char *datetime;
  char *task;
  int temp, vol, wifi, bright, fan;
  char *cpu_bar[CPU_NBR];

  int mem_percent;
  int mem_free;
  char* mem_free_color;
  char *mem_bar;

  char *fg_color = "#839496";
  char cpu_color[8];
  char mem_color[8];

  char bat0[256];
  
  const char CELSIUS_CHAR = (char)176;
  
  if (!(dpy = XOpenDisplay(NULL))) {
    fprintf(stderr, "Cannot open display.\n");
    return EXIT_FAILURE;
  }

  status = (char*) malloc(sizeof(char)*MSIZE);
  if(!status)
    return EXIT_FAILURE;

   while(1)
    {

	  mem_percent = getMemPercent();
	  mem_free = getMemFree();
      mem_free_color = getMemColor(mem_percent);
      percentColorGeneric(mem_color, mem_percent, 1);
	  mem_bar = hBar(mem_percent, 20, 9,  mem_color, "#444444");
      temp = getTemperature();
      fan = getFan();
      datetime = getDateTime();
      task = getActiveTask();
      getBatteryBar(bat0, 256, 30, 11);
      vol = getVolume();
      getCpuUsage(cpu_percent);
      wifi = getWifiPercent();
      bright = getBrightness();
      for(int i = 0; i < CPU_NBR; ++i)
      {
        percentColorGeneric(cpu_color, cpu_percent[i], 1);
	      cpu_bar[i] = vBar(cpu_percent[i], 2, 13, cpu_color, "#444444");
      }
      
      int ret = snprintf(
               status, 
               MSIZE, 
               "^c%s^%s[BRIGHT %d%%] [VOL %d%%] [CPU^f3^%s^f4^%s^f4^%s^f4^%s^f4^%s^f4^%s^f4^%s^f4^%s^f3^^c%s^ %d%cC/%drpm] [MEM ^c%s^%dMb^c%s^] [W %d] %s^c%s^ %s ", 
             fg_color,
             task,
             bright,
               vol, 
               cpu_bar[1],  
               cpu_bar[2],  
               cpu_bar[3],
               cpu_bar[4],
               cpu_bar[5],  
               cpu_bar[6],  
               cpu_bar[7],
               cpu_bar[8],
               fg_color,
               temp, CELSIUS_CHAR, 
               fan,
               mem_free_color,
               mem_free,
               fg_color,
               wifi,
               bat0, fg_color, datetime
               );
      if(ret >= MSIZE)
	fprintf(stderr, "error: buffer too small %d/%d\n", MSIZE, ret);

      free(datetime);
      free(task);
      for(int i = 0; i < CPU_NBR; ++i)
	      free(cpu_bar[i]);

      free(mem_bar);

      setStatus(dpy, status);
      sleep(1);
    }
   
  /* USELESS
  free(status);
  XCloseDisplay(dpy);
  
  return EXIT_SUCESS;
  */
}

/* *******************************************************************
 * FUNCTIONS
 ******************************************************************* */

char* vBar(int percent, int w, int h, char* fg_color, char* bg_color) 
{
  char *value;
  if((value = (char*) malloc(sizeof(char)*128)) == NULL)
    {
      fprintf(stderr, "Cannot allocate memory for buf.\n");
      exit(1);
    }
  char* format = "^c%s^^r0,%d,%d,%d^^c%s^^r0,%d,%d,%d^";

  int bar_height = (percent*h)/100;
  int y = (BAR_HEIGHT - h)/2;
  snprintf(value, 128, format, bg_color, y, w, h, fg_color, y + h-bar_height, w, bar_height);
  return value; 
}

char* hBar(int percent, int w, int h, char* fg_color, char* bg_color) 
{
  char *value;
  if((value = (char*) malloc(sizeof(char)*128)) == NULL)
    {
      fprintf(stderr, "Cannot allocate memory for buf.\n");
      exit(1);
    }
  char* format = "^c%s^^r0,%d,%d,%d^^c%s^^r0,%d,%d,%d^";

  int bar_width = (percent*w)/100;
  int y = (BAR_HEIGHT - h)/2;
  snprintf(value, 128, format, bg_color, y, w, h, fg_color, y, bar_width, h);
  return value; 
}

int hBar2(char *string, size_t size, int percent, int w, int h, char *fg_color, char *bg_color)
{
  char *format = "^c%s^^r0,%d,%d,%d^^c%s^^r%d,%d,%d,%d^";
  int bar_width = (percent*w)/100;

  int y = (BAR_HEIGHT - h)/2;
  return snprintf(string, size, format, fg_color, y, bar_width, h, bg_color, bar_width, y, w - bar_width, h);
}

int hBarBordered(char *string, size_t size, int percent, int w, int h, char *fg_color, char *bg_color, char *border_color)
{
	char tmp[128];
	hBar2(tmp, 128, percent, w - 2, h -2, fg_color, bg_color);
	int y = (BAR_HEIGHT - h)/2;
	char *format = "^c%s^^r0,%d,%d,%d^^f1^%s";
	return snprintf(string, size, format, border_color, y, w, h, tmp);
}

void 
setStatus(Display *dpy, char *str) 
{
  XStoreName(dpy, DefaultRootWindow(dpy), str);
  XSync(dpy, False);
}

void percentColorGeneric(char* string, int percent, int invert)
{
	char *format = "#%X0%X000";
	int a = (percent*15)/100;
	int b = 15 - a;
  if(!invert) {
    snprintf(string, 8, format, b, a);
  }
  else {
    snprintf(string, 8, format, a, b);
  }
}

void percentColor(char* string, int percent)
{
  percentColorGeneric(string, percent, 0);
}

int getBatteryBar(char *string, size_t size, int w, int h) 
{
  int percent = getBattery();
  
  char *bg_color = "#444444";
  char *border_color = "#839496";
  char fg_color[8];
  if(getBatteryStatus())
	  memcpy(fg_color, border_color, 8);
  else
	  percentColor(fg_color, percent);

  char tmp[128];
  hBarBordered(tmp, 128, percent, w -2, h, fg_color, bg_color, border_color);

  char *format = "%s^c%s^^f%d^^r0,%d,%d,%d^^f%d^";
  int y = (BAR_HEIGHT - 5)/2;
  return snprintf(string, size, format, tmp, border_color, w - 2, y, 2, 5, 2);
}

int 
getBattery()
{
  FILE *fd;
  int energy_now;

  static int energy_full = -1;
  if(energy_full == -1)
    {
      fd = fopen(BAT_FULL_FILE, "r");
      if(fd == NULL) {
        fprintf(stderr, "Error opening energy_full.\n");
        return -1;
      }
      fscanf(fd, "%d", &energy_full);
      fclose(fd);
    }
  
  fd = fopen(BAT_NOW_FILE, "r");
  if(fd == NULL) {
    fprintf(stderr, "Error opening energy_now.\n");
    return -1;
  }
  fscanf(fd, "%d", &energy_now);
  fclose(fd);
  
  return ((float)energy_now  / (float)energy_full) * 100;
}

int 
getBrightness()
{
  FILE *fd;
  int brightness_now;

  static int brightness_full = -1;
  if(brightness_full == -1)
    {
      fd = fopen(BRIGHT_FULL_FILE, "r");
      if(fd == NULL) {
        fprintf(stderr, "Error opening brightness_full.\n");
        return -1;
      }
      fscanf(fd, "%d", &brightness_full);
      fclose(fd);
    }
  
  fd = fopen(BRIGHT_NOW_FILE, "r");
  if(fd == NULL) {
    fprintf(stderr, "Error opening brightness_now.\n");
    return -1;
  }
  fscanf(fd, "%d", &brightness_now);
  fclose(fd);
  
  return ((float)brightness_now  / (float)brightness_full) * 100;
}

/** 
 * Return 0 if the battery is discharing, 1 if it's full or is 
 * charging, -1 if an error occured. 
 **/
int 
getBatteryStatus()
{
  FILE *fd;
  char first_letter;
  
  if( NULL == (fd = fopen(BAT_STATUS_FILE, "r")))
    return -1;

  fread(&first_letter, sizeof(char), 1, fd);
  fclose(fd);

  if(first_letter == 'D')
    return 0;

  return 1;
}

int
getMemPercent()
{
	FILE *fd;
	int mem_total;
	int mem_free;
	int mem_available;
	fd = fopen(MEMINFO_FILE, "r");
	if(fd == NULL) {
		fprintf(stderr, "Error opening energy_full.\n");
        return -1;
	}
	fscanf(fd, "MemTotal:%*[ ]%d kB\nMemFree:%*[ ]%d kB\nMemAvailable:%*[ ]%d", &mem_total, &mem_free, &mem_available);
	fclose (fd);
	return ((float)(mem_total-mem_available)/(float)mem_total) * 100;
}

int
getMemFree()
{
	FILE *fd;
	int mem_total;
	int mem_free;
	int mem_available;
	fd = fopen(MEMINFO_FILE, "r");
	if(fd == NULL) {
		fprintf(stderr, "Error opening energy_full.\n");
        return -1;
	}
	fscanf(fd, "MemTotal:%*[ ]%d kB\nMemFree:%*[ ]%d kB\nMemAvailable:%*[ ]%d", &mem_total, &mem_free, &mem_available);
	fclose (fd);
	return mem_free/1024;
}

char*
getMemColor(int percent)
{

    if ((percent >= 75) && (percent < 90)) {
        return "#b58900"; // solarized yellow
    } else if (percent >= 90) {
        return "#dc322f"; // solarized red
    } else {
        return "#839496";
    }
}

void
getCpuUsage(int* cpu_percent)
{
  size_t len = 0;
  char *line = NULL;
  int i;
  long int idle_time, other_time;
  char cpu_name[8]; 
  
  static int new_cpu_usage[CPU_NBR][4];
  static int old_cpu_usage[CPU_NBR][4];

  FILE *f;
  if(NULL == (f = fopen("/proc/stat", "r")))
    return;
  
  for(i = 0; i < CPU_NBR; ++i)
    {
      getline(&line,&len,f);
      sscanf(
             line, 
             "%s %d %d %d %d",
             cpu_name,
             &new_cpu_usage[i][0],
             &new_cpu_usage[i][1],
             &new_cpu_usage[i][2],
             &new_cpu_usage[i][3]
             );
      
      if(line != NULL)
        {
          free(line); 
          line = NULL;
        }

      idle_time = new_cpu_usage[i][3] - old_cpu_usage[i][3];
      other_time = new_cpu_usage[i][0] - old_cpu_usage[i][0]
        + new_cpu_usage[i][1] - old_cpu_usage[i][1]
        + new_cpu_usage[i][2] - old_cpu_usage[i][2];
      
      if(idle_time + other_time != 0)
        cpu_percent[i] = (other_time*100)/(idle_time + other_time);
      else
        cpu_percent[i] = 0; 

      old_cpu_usage[i][0] = new_cpu_usage[i][0];
      old_cpu_usage[i][1] = new_cpu_usage[i][1];
      old_cpu_usage[i][2] = new_cpu_usage[i][2];
      old_cpu_usage[i][3] = new_cpu_usage[i][3];
    }
    
    fclose(f);
}

float 
getFreq(char *file)
{
  FILE *fd;
  char *freq; 
  float ret;
  
  freq = (char*) malloc(10);
  fd = fopen(file, "r");
  if(fd == NULL)
    {
      fprintf(stderr, "Cannot open '%s' for reading.\n", file);
      exit(1);
    }

  fgets(freq, 10, fd);
  fclose(fd);

  ret = atof(freq)/1000000;
  free(freq);
  return ret;
}

char *
getDateTime() 
{
  char *buf;
  time_t result;
  struct tm *resulttm;
  
  if((buf = (char*) malloc(sizeof(char)*65)) == NULL)
    {
      fprintf(stderr, "Cannot allocate memory for buf.\n");
      exit(1);
    }

  result = time(NULL);
  resulttm = localtime(&result);
  if(resulttm == NULL)
    {
      fprintf(stderr, "Error getting localtime.\n");
      exit(1);
    }
  
  if(!strftime(buf, sizeof(char)*65-1, "[%a %b %d] [%H:%M]", resulttm))
    {
      fprintf(stderr, "strftime is 0.\n");
      exit(1);
    }
	
  return buf;
}

char *concat(const char *s1, const char *s2)
{
    char *result = malloc(strlen(s1) + strlen(s2) + 1); // +1 for the null-terminator
    strcpy(result, s1);
    strcat(result, s2);

    return result;
}

char *getActiveTask() 
{
    char *buf1, *buf2, *buf3;
    buf1 = (char*) malloc(sizeof(char)*BUFFER_FOR_TASK);
    buf2 = (char*) malloc(sizeof(char)*BUFFER_FOR_TASK);
    buf3 = (char*) malloc(sizeof(char)*BUFFER_FOR_TASK);

    char *description, *id, *start;
    description = (char*) malloc(sizeof(char)*BUFFER_FOR_TASK);
    id = (char*) malloc(sizeof(char)*BUFFER_FOR_TASK);
    start = (char*) malloc(sizeof(char)*BUFFER_FOR_TASK);

    FILE *fp = popen("task +ACTIVE export", "r");
    char buffer[1024];
    char *json_string = "";
    while (fgets(buffer, sizeof(buffer), fp)) {
        json_string = concat(json_string, buffer);
    }
    pclose(fp);

    struct json_object *obj;
    struct json_object *obj_desription;
    struct json_object *obj_id;
    struct json_object *obj_start;

    obj = json_tokener_parse(json_string);
    if (json_object_get_type(obj) == json_type_array) {
        obj = json_object_array_get_idx(obj, 0);
    } else if (json_object_get_type(obj) == json_type_object) {
        printf("EROOR: json object - not array\n");
    }

    obj_desription = json_object_object_get(obj, "description");
    obj_id = json_object_object_get(obj, "id");
    obj_start = json_object_object_get(obj, "start");

    if (obj == NULL) {
        sprintf(buf3, "");
    } else {
        description = json_object_to_json_string(obj_desription);
        id = json_object_to_json_string(obj_id);
        start = json_object_to_json_string(obj_start);

        sprintf(buf1, "%s", description);
        snprintf(buf2, strlen(buf1) - 1, "%s", &buf1[1]);
        sprintf(buf3, "[%s ^c#ffffff^%s^d^] ", id, buf2);
        //sprintf(buf3, "[%s ^c#ffffff^%s^d^ %s] ", id, buf2, "1h9m");
    }

    //printf("%s\n", buf3);
    return buf3;
}

int
getTemperature()
{
  int temp;
  FILE *fd = fopen(TEMP_SENSOR_FILE, "r");
  if(fd == NULL) 
	  {
		  fprintf(stderr, "Error opening temp1_input.\n");
		  return -1;
	  }
  fscanf(fd, "%d", &temp);
  fclose(fd);
  
  return temp / 1000;
}

int
getFan()
{
  int fan;
  FILE *fd = fopen(FAN_SENSOR_FILE, "r");
  if(fd == NULL) 
	  {
		  fprintf(stderr, "Error opening fan1_input.\n");
		  return -1;
	  }
  fscanf(fd, "%d", &fan);
  fclose(fd);
  
  return fan;
}

int
getWifiPercent()
{
	//size_t len = 0;
	int percent = 0;
	//char line[512] = {'\n'};
	FILE *fd = fopen("/proc/net/wireless", "r");
	if(fd == NULL)
		{
			fprintf(stderr, "Error opening wireless info");
			return -1;
		}
	fscanf(fd, "%*[^\n]\n%*[^\n]\n%*s %*[0-9] %d", &percent);
	fclose(fd);
	return percent;
}
	

int
getVolume()
{
  const char* MIXER = "Master";
  /* OSS
  const char* OSSMIXCMD = "ossmix vmix0-outvol";
  const char* OSSMIXFORMAT = "Value of mixer control vmix0-outvol is currently set to %f (dB)";
  */

  float vol = 0;
  long pmin, pmax, pvol;

  /* Alsa {{{ */
  snd_mixer_t *handle;
  snd_mixer_selem_id_t *sid;
  snd_mixer_elem_t *elem;
  snd_mixer_selem_id_alloca(&sid);

  if(snd_mixer_open(&handle, 0) < 0)
    return 0;

  if(snd_mixer_attach(handle, "default") < 0
     || snd_mixer_selem_register(handle, NULL, NULL) < 0
     || snd_mixer_load(handle) > 0)
    {
      snd_mixer_close(handle);
      return 0;
    }

  for(elem = snd_mixer_first_elem(handle); elem; elem = snd_mixer_elem_next(elem))
    {
      snd_mixer_selem_get_id(elem, sid); 
      if(!strcmp(snd_mixer_selem_id_get_name(sid), MIXER))
        {
          snd_mixer_selem_get_playback_volume_range(elem, &pmin, &pmax);
          snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_MONO, &pvol);
          vol = ((float)pvol / (float)(pmax - pmin)) * 100;
        }
    }

  snd_mixer_close(handle);
  /* }}} */
  /* Oss (soundcard.h not working) {{{ 
     if(!(f = popen(OSSMIXCMD, "r")))
       return;

     fscanf(f, OSSMIXFORMAT, &vol);
     pclose(f);
     }}} */

  return vol;
}

