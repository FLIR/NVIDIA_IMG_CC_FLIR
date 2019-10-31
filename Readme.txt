/* ------------------------------------------------------------------------- */
/* FLIR Systems, Inc                                             June 2017   */
/* ------------------------------------------------------------------------- */
/* NVMIMG_CC application tuned by FLIR to demonstrate ADK GMSL kit for AGX   */
/* This code was based on the NVDIA VibranteSDK version 4.1.4.0  (Mar2017)   */
/* provided by NVIDIA corporation as part of the AGX development kit.        */
/* It uses NvMedia framework to capture and display the video                */
/*                                                                           */
/* It works for ADK_Boson320 and ADK_Boson640. capturing, displaying and     */
/* recording frames.                                                         */
/*                                                                           */
/* ------------------------------------------------------------------------- */

/* NVIDIA CORPORATION gave permission to FLIR Systems, Inc to modify this code
 * and distribute it as part of the ADK GMSL Kit.
 * http://www.flir.com/cores/content/?id=82274
 * This file includes some FLIR changes noted as @@@@
 * June-2017
 *
 * The files that got FLIR modifications are: save.c and capture.c
 *
*/

/* ------------------------------------------------------------------------- */
/*    Install / Build instructions:                                          */
/* ------------------------------------------------------------------------- */

1. Go into NVMedia Samples:
   cd (PATH)/VibranteSDK/vibrante-t186ref-linux/samples/nvmedia/

2. copy img_cc_flir.tgz in that folder

3. untar the file
   tar xvfz img_cc_flir.tgz

4. cd img_cc_flir

5. compile
   make clean
   make

6. copy nvmedia_cc file into AGX
   cp boson/boson640.script into AGX

7. Run the application in the AGX
   - It assumes that ADK kit is connected to port A0 on AGX
   - ./nvmimg_cc_flir -wrregs boson640.script
      (if it works you will see the capture '/,-.\,|' icon moving )

   - ./nvmimg_cc_flir -wrregs boson640.script -d 0 -w 1 -p 10:10:320:257
       ( does the same plus displays the video (*) )

   - ./nvmimg_cc_flir -wrregs boson640.script -d 0 -w 1 -p 10:10:320:257 -f filename - n X   ( X = number of frames to capture )
	     FLIR includes the tool 'displayRaw' to display the captured image.

8. Modifications done by FLIR
   capture.c :
	- Added definition of RAW14
	- Remove the error checking (**)

   save.c :
	- Re-ordering of bits

   display.c :
    - Checks for ffc command

9. Added functionality:
    - Trigger FFC by entering 'f' in terminal followed by 'enter' 
        while the video is streaming

(**) Due to the misalignment of surfaces the video is dropping some frames, what
     causes an error. After 11 errors , NVIDIA quits the application, FLIR case
     keeps running. If Display is not enabled the capture works.


/* ------------------------------------------------------------------------- */
/* Future work:                                                              */
/*     - Fix surface and render full image                                   */
/*     - add option to know if Telemetry is ON / OFF                         */
/*     - fix frame drops when displaying video using nvmedia                 */
/*     - Avoid (at source) bit re-ordering                                   */
/*                                                                           */
/* ------------------------------------------------------------------------- */
