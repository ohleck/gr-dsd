/* -*- c++ -*- */
/*
 * Copyright 2004,2010 Free Software Foundation, Inc.
 *
 * This file is part of GNU Radio
 *
 * GNU Radio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * GNU Radio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Radio; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

/*
 * config.h is generated by configure.  It contains the results
 * of probing for features, options etc.  It should be the first
 * file included in your .cc file.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <dsd_block_ff.h>
#include <gr_io_signature.h>


#include <sys/syscall.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

/*
 * Create a new instance of dsd_block_ff and return
 * a boost shared_ptr.  This is effectively the public constructor.
 */
dsd_block_ff_sptr
dsd_make_block_ff (dsd_frame_mode frame, dsd_modulation_optimizations mod, int uvquality, bool errorbars, int verbosity, bool empty, int num)
{
  return gnuradio::get_initial_sptr(new dsd_block_ff (frame, mod, uvquality, errorbars, verbosity, empty, num));
}

/*
 * Specify constraints on number of input and output streams.
 * This info is used to construct the input and output signatures
 * (2nd & 3rd args to gr_block's constructor).  The input and
 * output signatures are used by the runtime system to
 * check that a valid number and type of inputs and outputs
 * are connected to this block.  In this case, we accept
 * only 1 input and 1 output.
 */
static const int MIN_IN = 1;	// mininum number of input streams
static const int MAX_IN = 1;	// maximum number of input streams
static const int MIN_OUT = 1;	// minimum number of output streams
static const int MAX_OUT = 1;	// maximum number of output streams


void cleanupHandler(void *arg) {
dsd_params *params = (dsd_params *) arg;

pthread_mutex_destroy(&params->state.output_mutex);
pthread_mutex_destroy(&params->state.input_mutex);
pthread_cond_destroy(&params->state.input_ready);
pthread_cond_destroy(&params->state.output_ready);
printf(" - Pthread destructor [ %d ] \n", params->num);

}

void* run_dsd (void *arg)
{
  dsd_params *params = (dsd_params *) arg;
  pthread_cleanup_push(cleanupHandler, arg);
  //openWavOutFile (&params->opts, &params->state);
  liveScanner (&params->opts, &params->state);
  pthread_cleanup_pop(0);
  return NULL;
}

void dsd_block_ff::reset_state(){
  dsd_state *state = &params.state;
  memset (state->src_list, 0, sizeof (long) * 50);
  memset (state->xv, 0, sizeof (float) * (NZEROS+1));
  memset (state->nxv, 0, sizeof (float) * (NXZEROS+1));
  state->debug_audio_errors = 0;
  state->debug_header_errors = 0;
  state->debug_header_critical_errors = 0;
  state->symbolcnt = 0;
  /*printf("\n");
  printf("+P25 BER estimate: %.2f%%\n", get_P25_BER_estimate(&state->p25_heuristics));
  printf("-P25 BER estimate: %.2f%%\n", get_P25_BER_estimate(&state->inv_p25_heuristics));
  printf("\n");*/
  initialize_p25_heuristics(&state->p25_heuristics);
}

dsd_state *dsd_block_ff::get_state()
{

  return &params.state;
}

/*
 * The private constructor
 */

dsd_block_ff::dsd_block_ff (dsd_frame_mode frame, dsd_modulation_optimizations mod, int uvquality, bool errorbars, int verbosity, bool empty, int num)
  : gr_block ("block_ff",
	      gr_make_io_signature (MIN_IN, MAX_IN, sizeof (float)),
	      gr_make_io_signature (MIN_OUT, MAX_OUT, sizeof (float)))
{
  initOpts (&params.opts);
  initState (&params.state);
  pthread_attr_t tattr;

  struct sched_param param;
  int pr,error,i, policy;
  params.num = num;
  params.opts.split = 1;
  params.opts.playoffset = 0;
  params.opts.delay = 0;

  if (frame == dsd_FRAME_AUTO_DETECT)
  {
    params.opts.frame_dstar = 0;
    params.opts.frame_x2tdma = 1;
    params.opts.frame_p25p1 = 1;
    params.opts.frame_nxdn48 = 0;
    params.opts.frame_nxdn96 = 1;
    params.opts.frame_dmr = 1;
    params.opts.frame_provoice = 0;
  }
  else if (frame == dsd_FRAME_DSTAR)
  {
    params.opts.frame_dstar = 1;
    params.opts.frame_x2tdma = 0;
    params.opts.frame_p25p1 = 0;
    params.opts.frame_nxdn48 = 0;
    params.opts.frame_nxdn96 = 0;
    params.opts.frame_dmr = 0;
    params.opts.frame_provoice = 0;
    printf ("Decoding only D-STAR frames.\n");
  }
  else if (frame == dsd_FRAME_X2_TDMA)
  {
    params.opts.frame_dstar = 0;
    params.opts.frame_x2tdma = 1;
    params.opts.frame_p25p1 = 0;
    params.opts.frame_nxdn48 = 0;
    params.opts.frame_nxdn96 = 0;
    params.opts.frame_dmr = 0;
    params.opts.frame_provoice = 0;
    printf ("Decoding only X2-TDMA frames.\n");
  }
  else if (frame == dsd_FRAME_PROVOICE)
  {
    params.opts.frame_dstar = 0;
    params.opts.frame_x2tdma = 0;
    params.opts.frame_p25p1 = 0;
    params.opts.frame_nxdn48 = 0;
    params.opts.frame_nxdn96 = 0;
    params.opts.frame_dmr = 0;
    params.opts.frame_provoice = 1;
    params.state.samplesPerSymbol = 5;
    params.state.symbolCenter = 2;
    params.opts.mod_c4fm = 0;
    params.opts.mod_qpsk = 0;
    params.opts.mod_gfsk = 1;
    params.state.rf_mod = 2;
    printf ("Setting symbol rate to 9600 / second\n");
    printf ("Enabling only GFSK modulation optimizations.\n");
    printf ("Decoding only ProVoice frames.\n");
  }
  else if (frame == dsd_FRAME_P25_PHASE_1)
  {
    params.opts.frame_dstar = 0;
    params.opts.frame_x2tdma = 0;
    params.opts.frame_p25p1 = 1;
    params.opts.frame_nxdn48 = 0;
    params.opts.frame_nxdn96 = 0;
    params.opts.frame_dmr = 0;
    params.opts.frame_provoice = 0;
    printf ("Decoding only P25 Phase 1 frames.\n");
  }
  else if (frame == dsd_FRAME_NXDN48_IDAS)
  {
    params.opts.frame_dstar = 0;
    params.opts.frame_x2tdma = 0;
    params.opts.frame_p25p1 = 0;
    params.opts.frame_nxdn48 = 1;
    params.opts.frame_nxdn96 = 0;
    params.opts.frame_dmr = 0;
    params.opts.frame_provoice = 0;
    params.state.samplesPerSymbol = 20;
    params.state.symbolCenter = 10;
    params.opts.mod_c4fm = 0;
    params.opts.mod_qpsk = 0;
    params.opts.mod_gfsk = 1;
    params.state.rf_mod = 2;
    printf ("Setting symbol rate to 2400 / second\n");
    printf ("Enabling only GFSK modulation optimizations.\n");
    printf ("Decoding only NXDN 4800 baud frames.\n");
  }
  else if (frame == dsd_FRAME_NXDN96)
  {
    params.opts.frame_dstar = 0;
    params.opts.frame_x2tdma = 0;
    params.opts.frame_p25p1 = 0;
    params.opts.frame_nxdn48 = 0;
    params.opts.frame_nxdn96 = 1;
    params.opts.frame_dmr = 0;
    params.opts.frame_provoice = 0;
    params.opts.mod_c4fm = 0;
    params.opts.mod_qpsk = 0;
    params.opts.mod_gfsk = 1;
    params.state.rf_mod = 2;
    printf ("Enabling only GFSK modulation optimizations.\n");
    printf ("Decoding only NXDN 9600 baud frames.\n");
  }
  else if (frame == dsd_FRAME_DMR_MOTOTRBO)
  {
    params.opts.frame_dstar = 0;
    params.opts.frame_x2tdma = 0;
    params.opts.frame_p25p1 = 0;
    params.opts.frame_nxdn48 = 0;
    params.opts.frame_nxdn96 = 0;
    params.opts.frame_dmr = 1;
    params.opts.frame_provoice = 0;
    printf ("Decoding only DMR/MOTOTRBO frames.\n");
  }

  params.opts.uvquality = uvquality;
 params.opts.verbose = verbosity;
    params.opts.errorbars = errorbars;



/*
 params.opts.verbose = 0;//verbosity;
    params.opts.errorbars = 0;//errorbars;
if (errorbars){

    params.opts.datascope = 1;
}
*/
  empty_frames = empty;

  if (mod == dsd_MOD_AUTO_SELECT)
  {
    params.opts.mod_c4fm = 1;
    params.opts.mod_qpsk = 1;
    params.opts.mod_gfsk = 1;
    params.state.rf_mod = 0;
  }
  else if (mod == dsd_MOD_C4FM)
  {
    params.opts.mod_c4fm = 1;
    params.opts.mod_qpsk = 0;
    params.opts.mod_gfsk = 0;
    params.state.rf_mod = 0;
    printf ("Enabling only C4FM modulation optimizations.\n");
  }
  else if (mod == dsd_MOD_GFSK)
  {
    params.opts.mod_c4fm = 0;
    params.opts.mod_qpsk = 0;
    params.opts.mod_gfsk = 1;
    params.state.rf_mod = 2;
    printf ("Enabling only GFSK modulation optimizations.\n");
  }
  else if (mod == dsd_MOD_QPSK)
  {
    params.opts.mod_c4fm = 0;
    params.opts.mod_qpsk = 1;
    params.opts.mod_gfsk = 0;
    params.state.rf_mod = 1;
    printf ("Enabling only QPSK modulation optimizations.\n");
  }

  // Initialize the mutexes
  if(pthread_mutex_init(&params.state.input_mutex, NULL))
  {
    printf("Unable to initialize input mutex\n");
  }
  if(pthread_mutex_init(&params.state.output_mutex, NULL))
  {
    printf("Unable to initialize output mutex\n");
  }
  if(pthread_mutex_init(&params.state.quit_mutex, NULL))
  {
    printf("Unable to initialize quit mutex\n");
  }

  // Initialize the conditions
  if(pthread_cond_init(&params.state.input_ready, NULL))
  {
    printf("Unable to initialize input condition\n");
  }
  if(pthread_cond_init(&params.state.output_ready, NULL))
  {
    printf("Unable to initialize output condition\n");
  }
  // Lock output mutex
  if (pthread_mutex_lock(&params.state.output_mutex))
  {
    printf("Unable to lock mutex\n");
  }
  if (!empty_frames) {
  	set_output_multiple(160);
  }
  params.state.input_length = 0;

  params.state.output_buffer = (short *) malloc(4 * 80000); // TODO: Make this variable size.
  params.state.output_offset = 0;
  if (params.state.output_buffer == NULL)
  {
    printf("Unable to allocate output buffer.\n");
  }

  //strcpy(params.opts.wav_out_file, "dsd.wav");

if(error=pthread_attr_init(&tattr))
{
    fprintf(stderr,"Attribute initialization failed with error %s\n",strerror(error));
}


//I don't understand schedule policy
/*
policy = SCHED_RR;

    error = pthread_attr_setschedpolicy(&tattr, policy);

    // insert error handling

 error = pthread_attr_getschedparam(&tattr,&param);

        if(error!=0)
        {
            printf("failed to get priority\n");
        }

        param.sched_priority=10;
        error=pthread_attr_setschedparam(&tattr,&param);

        if(error!=0)
        {
            printf("failed to set priority\n");
        }
*/


pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);

  if(pthread_create(&dsd_thread, &tattr, &run_dsd, &params))
  {
    printf("Unable to spawn thread\n");
  }
pthread_attr_destroy(&tattr);

}
int dsd_block_ff::close () {




}

/*
 * Our virtual destructor.
 */
dsd_block_ff::~dsd_block_ff ()
{




pthread_cancel(dsd_thread);
usleep(1000*1000);

//printf("dsd_block_ff.cc: freeing output buffer!\n");
free(params.state.output_buffer);


  //printf("dsd_block_ff: Trying to free memory/ \n");


  free(params.state.dibit_buf);
  free(params.state.audio_out_buf);
  free(params.state.audio_out_float_buf);
  free(params.state.cur_mp);
  free(params.state.prev_mp);
  free(params.state.prev_mp_enhanced);


  printf(" - dsd_block_ff destructor [ %d ] \n", params.num);

}

int
dsd_block_ff::general_work (int noutput_items,
			gr_vector_int &ninput_items,
			gr_vector_const_void_star &input_items,
			gr_vector_void_star &output_items)
{
  int i;
  int send_to_dsd = 0;

  const float *in = (const float *) input_items[0];
  float *out = (float *) output_items[0];
//memcpy(out, in, ninput_items[0] * sizeof(float));

  params.state.output_samples = out;
  params.state.output_num_samples = 0;
  params.state.output_length = noutput_items;
  params.state.output_finished = 0;

  if (pthread_mutex_lock(&params.state.input_mutex))
  {
    printf("Unable to lock mutex\n");
  }
  params.state.input_samples = in;
  params.state.input_length = ninput_items[0];
  params.state.input_offset = 0;

  if (pthread_cond_signal(&params.state.input_ready))
  {
    printf("Unable to signal\n");
  }

  if (pthread_mutex_unlock(&params.state.input_mutex))
  {
    printf("Unable to unlock mutex\n");
  }

  while (params.state.output_finished == 0)
  {
    if (pthread_cond_wait(&params.state.output_ready, &params.state.output_mutex))
    {
      printf("general_work -> Error waiting for condition\n");
    }
  }



 if (params.state.output_num_samples > 0) {
	printf("[%lu] \tInputs: %d \tReq Outputs: %d \tOutputs: %d \t Buffer Offset: %d\n",long(pthread_self()),ninput_items[0],noutput_items, params.state.output_num_samples, params.state.output_offset);

}

if (empty_frames) {

  this->consume(0, ninput_items[0]);

return (noutput_items);
} else {

  this->consume(0, ninput_items[0]);

  return params.state.output_num_samples;

if ((params.state.output_num_samples > 0) && (params.state.output_num_samples < noutput_items)) {

 return 0;
} else {

  this->consume(0, ninput_items[0]);

  return params.state.output_num_samples;
}

}
/*
this->consume(0, ninput_items[0]);
return ninput_items[0];
*/
}
