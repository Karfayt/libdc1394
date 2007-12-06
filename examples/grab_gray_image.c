/**************************************************************************
**       Title: grab one gray image using libdc1394
**    $RCSfile$
**   $Revision$$Name$
**       $Date$
**   Copyright: LGPL $Author$
** Description:
**
**    Get one gray image using libdc1394 and store it as portable gray map
**    (pgm). Based on 'samplegrab' from Chris Urmson 
**
**************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <dc1394/utils.h>
#include <dc1394/control.h>
#include <stdlib.h>
#include <inttypes.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#define IMAGE_FILE_NAME "image.pgm"

/*-----------------------------------------------------------------------
 *  Releases the cameras and exits
 *-----------------------------------------------------------------------*/
void cleanup_and_exit(dc1394camera_t *camera)
{
  dc1394_video_set_transmission(camera, DC1394_OFF);
  dc1394_capture_stop(camera);
  dc1394_camera_free(camera);
  exit(1);
}

int main(int argc, char *argv[]) 
{
  FILE* imagefile;
  dc1394camera_t *camera;
  int i;
  dc1394featureset_t features;
  dc1394framerates_t framerates;
  dc1394video_modes_t video_modes;
  dc1394framerate_t framerate;
  dc1394video_mode_t video_mode = 0;
  dc1394color_coding_t coding;
  unsigned int width, height;
  dc1394video_frame_t *frame;
  dc1394_t * d;
  dc1394camera_list_t * list;

  d = dc1394_new ();
  if (dc1394_camera_enumerate (d, &list) != DC1394_SUCCESS) {
    fprintf (stderr, "Failed to enumerate cameras\n");
    return 1;
  }

  if (list->num == 0) {
    fprintf (stderr, "No cameras found\n");
    return 1;
  }
  
  camera = dc1394_camera_new (d, list->ids[0].guid);
  if (!camera) {
    fprintf (stderr, "Failed to initialize camera with guid %"PRIx64"\n",
        list->ids[0].guid);
    return 1;
  }
  dc1394_camera_free_list (list);

  printf("Using camera with GUID %"PRIx64"\n", camera->guid);
  
  /*-----------------------------------------------------------------------
   *  get the best video mode and highest framerate. This can be skipped
   *  if you already know which mode/framerate you want...
   *-----------------------------------------------------------------------*/
  // get video modes:
  if (dc1394_video_get_supported_modes(camera,&video_modes)!=DC1394_SUCCESS) {
    fprintf(stderr,"Can't get video modes\n");
    cleanup_and_exit(camera);
  }

  // select highest res mode:
  for (i=video_modes.num-1;i>=0;i--) {
    if (!dc1394_is_video_mode_scalable(video_modes.modes[i])) {
      dc1394_get_color_coding_from_video_mode(camera,video_modes.modes[i], &coding);
      if (coding==DC1394_COLOR_CODING_MONO8) {
	video_mode=video_modes.modes[i];
	break;
      }
    }
  }
  if (i < 0) {
    fprintf(stderr,"Could not get a valid MONO8 mode\n");
    cleanup_and_exit(camera);
  }

  if (dc1394_get_color_coding_from_video_mode(camera, video_mode,
        &coding) != DC1394_SUCCESS) {
    fprintf (stderr, "Can't get color coding\n");
    cleanup_and_exit(camera);
  }

  // get highest framerate
  if (dc1394_video_get_supported_framerates(camera,video_mode,&framerates)!=DC1394_SUCCESS) {
    fprintf(stderr,"Can't get framrates\n");
    cleanup_and_exit(camera);
  }
  framerate=framerates.framerates[framerates.num-1];
    
  /*-----------------------------------------------------------------------
   *  setup capture
   *-----------------------------------------------------------------------*/
  fprintf(stderr,"Setting capture\n");

  dc1394_video_set_iso_speed(camera, DC1394_ISO_SPEED_400);
  dc1394_video_set_mode(camera, video_mode);
  dc1394_video_set_framerate(camera, framerate);
  if (dc1394_capture_setup(camera,4, DC1394_CAPTURE_FLAGS_DEFAULT)!=DC1394_SUCCESS) {
    fprintf( stderr,"unable to setup camera-\n"
             "check line %d of %s to make sure\n"
             "that the video mode and framerate are\n"
             "supported by your camera\n",
             __LINE__,__FILE__);
    cleanup_and_exit(camera);
  }
  
  /*-----------------------------------------------------------------------
   *  report camera's features
   *-----------------------------------------------------------------------*/
  if (dc1394_feature_get_all(camera,&features) !=DC1394_SUCCESS) {
    fprintf( stderr, "unable to get feature set\n");
  }
  else {
    dc1394_feature_print_all(&features, stdout);
  }
    

  fprintf(stderr,"start transmission\n");
  /*-----------------------------------------------------------------------
   *  have the camera start sending us data
   *-----------------------------------------------------------------------*/
  if (dc1394_video_set_transmission(camera, DC1394_ON) !=DC1394_SUCCESS) {
    fprintf( stderr, "unable to start camera iso transmission\n");
    cleanup_and_exit(camera);
  }

  fprintf(stderr,"wait transmission\n");
  /*-----------------------------------------------------------------------
   *  Sleep untill the camera has a transmission
   *-----------------------------------------------------------------------*/
  dc1394switch_t status = DC1394_OFF;

  i = 0;
  while( status == DC1394_OFF && i++ < 5 ) {
    usleep(50000);
    if (dc1394_video_get_transmission(camera, &status)!=DC1394_SUCCESS) {
      fprintf(stderr, "unable to get transmision status\n");
      cleanup_and_exit(camera);
    }
  }

  if( i == 5 ) {
    fprintf(stderr,"Camera doesn't seem to want to turn on!\n");
    cleanup_and_exit(camera);
  }

  fprintf(stderr,"capture\n");
  /*-----------------------------------------------------------------------
   *  capture one frame
   *-----------------------------------------------------------------------*/
  if (dc1394_capture_dequeue(camera, DC1394_CAPTURE_POLICY_WAIT, &frame)!=DC1394_SUCCESS) {
    fprintf(stderr, "unable to capture a frame\n");
    cleanup_and_exit(camera);
  }
  
  fprintf(stderr,"stop transmission\n");
  /*-----------------------------------------------------------------------
   *  Stop data transmission
   *-----------------------------------------------------------------------*/
  if (dc1394_video_set_transmission(camera,DC1394_OFF)!=DC1394_SUCCESS) {
    printf("couldn't stop the camera?\n");
  }
  
  /*-----------------------------------------------------------------------
   *  save image as 'Image.pgm'
   *-----------------------------------------------------------------------*/
  imagefile=fopen(IMAGE_FILE_NAME, "wb");

  if( imagefile == NULL) {
    perror( "Can't create '" IMAGE_FILE_NAME "'");
    cleanup_and_exit(camera);
  }
  
  dc1394_get_image_size_from_video_mode(camera, video_mode, &width, &height);
  fprintf(imagefile,"P5\n%u %u 255\n", width, height);
  fwrite(frame->image, 1, height*width, imagefile);
  fclose(imagefile);
  printf("wrote: " IMAGE_FILE_NAME "\n");

  /*-----------------------------------------------------------------------
   *  Close camera
   *-----------------------------------------------------------------------*/
  dc1394_video_set_transmission(camera, DC1394_OFF);
  dc1394_capture_stop(camera);
  dc1394_camera_free(camera);
  dc1394_free (d);

  return 0;
}
