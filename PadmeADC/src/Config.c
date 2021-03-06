#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "regex.h"

#include "Config.h"

#define MAX_PARAM_NAME_LEN  128
#define MAX_PARAM_VALUE_LEN 1024

config_t* Config; // Allocate pointer to common configuration structure

// Initialiaze configuration structure and set it to default
int init_config()
{

  Config = (config_t*)malloc(sizeof(config_t));
  if (Config == NULL) {
    printf("*** ERROR *** Memory allocation for configuration structure failed.\n");
    return 1;
  }
  memset(Config,0,sizeof(config_t));
  if ( reset_config() ) {
    printf("*** ERROR *** Problem setting run configuration to default values.\n");
    return 1;
  }
  return 0;

}

// Set configuration parameters to the default values
int reset_config()
{

  int ch;

  Config->process_id = 0;

  strcpy(Config->process_mode,"DAQ"); // Work in DAQ mode

  strcpy(Config->config_file,"");

  strcpy(Config->quit_file,"run/quit");
  strcpy(Config->start_file,"run/start");
  strcpy(Config->initok_file,"run/initok.b00"); // InitOK file for default board 0
  strcpy(Config->initfail_file,"run/initfail.b00"); // InitFail file for default board 0
  strcpy(Config->lock_file,"run/lock.b00"); // Lock file for default board 0

  Config->run_number = 0; // Dummy run (no DB access)

  strcpy(Config->input_stream,""); // No input stream defined for DAQ mode

  strcpy(Config->output_mode,"FILE"); // Default to old functioning mode (write to file)

  strcpy(Config->output_stream,""); // No output stream defined when in FILE mode

  // In FILE mode all data files written to subdirectory "data" of current directory
  strcpy(Config->data_dir,"data/");
  strcpy(Config->data_file,"daq_b00"); // Data filename template for default board 0

  Config->total_daq_time = 0; // Run forever
  //Config->total_daq_time = 2; // While testing use a 2s run time

  Config->board_id = 0; // Board 0 is the default board used for testing

  Config->board_sn = 0; // Default to dummy board serial number

  // board_id as default to set connection info
  strcpy(Config->connect_mode,"USB"); // Default is USB connection
  Config->conet2_link = 0;
  Config->conet2_slot = 0; // ignored for USB

  Config->startdaq_mode = 0; // Default to SW controlled start/stop

  Config->drs4_sampfreq = 2; // Default to 1GHz sampling frequency

  Config->trigger_mode = 1; // Use fast trigger mode (0:ext trigger, 1: fast trigger, 2:sw trigger)

  strcpy(Config->trigger_iolevel,"NIM"); // Triggers expect NIM levels (NIM or TTL)

  Config->group_enable_mask = 0xf; // All groups are ON
  //Config->group_enable_mask = 0x1; // Only group 0 is ON (ch 0-7)
  //Config->group_enable_mask = 0x3; // Only groups 0 and 1 are ON (ch 0-7 and 8-15)

  Config->channel_enable_mask = 0xffffffff; // All channels are ON

  // All offsets are at default value of 0x5600
  Config->offset_global = 0x5600;
  for(ch=0;ch<32;ch++) Config->offset_ch[ch] = Config->offset_global;

  // Delay of trigger wrt start of sample
  Config->post_trigger_size = 65;

  // Use digitizer buffer up to its full size (128 events for V1742, 1024 for V1742B)
  Config->max_num_events_blt = 128;

  // Enable DRS4 corrections to sampled data
  Config->drs4corr_enable = 1;

  // Add a delay between successive polls to the board
  Config->daq_loop_delay = 10000; // wait 10 msec after each iteration

  // Apply zero-suppression algorithm 2 in flagging mode (test phase, this default will change in production)
  Config->zero_suppression = 102;

  // Set default parameters for zero-suppression algorithm 1
  Config->zs1_head = 80; // Use first 80 samples to compute mean and rms
  Config->zs1_tail = 30; // Do not use final 30 samples for zero suppression
  Config->zs1_nsigma = 3.; // Threshold set a mean +/- 3*rms
  Config->zs1_nabovethr = 4; // Require at least 4 consecutive samples above threshold to accept the channel
  Config->zs1_badrmsthr = 15.; // If rms is above 15 counts, channel has problems and is accepted

  // Set default parameters for zero-suppression algorithm 2
  Config->zs2_tail = 30; // Do not use final 30 samples for zero suppression
  Config->zs2_minrms = 4.6;
  for(ch=0;ch<32;ch++) Config->zs2_minrms_ch[ch] = Config->zs2_minrms;

  // Set default parameters for trigger-based autopass system
  Config->auto_threshold = 0x0400; // Threshold below which trigger is considered ON (usual levels are 0x0800/0x0100)
  Config->auto_duration = 150; // Trigger ON duration (in ns) after which autopass is enabled

  // Ouput file limits
  Config->file_max_duration = 900; // 15 min
  Config->file_max_size = 1024*1024*1024; // 1GiB
  Config->file_max_events = 100000; // 1E5 events

  // Rate of debug output (1=all events)
  Config->debug_scale = 100; // Info about one event on 100 is written to debug output

  return 0;

}

int read_config(char *cfgfile)
{

  FILE *fin;
  char *line = NULL;
  size_t len = 0;
  ssize_t lsize;
  char param[MAX_PARAM_NAME_LEN];
  char value[MAX_PARAM_VALUE_LEN];
  size_t plen,vlen;
  int32_t v;
  //int64_t vl;
  uint32_t vu;
  uint64_t vul;
  float vf;
  int ch;

  regex_t rex_empty;
  regex_t rex_comment;
  regex_t rex_setting;
  regex_t rex_chsetting;
  int rex_err;
  size_t nm = 3;
  regmatch_t rm[3];
  size_t ncm = 4;
  regmatch_t rcm[4];

  // Define needed regular expressions
  if ( (rex_err = regcomp(&rex_empty,"^[[:blank:]]*$",REG_EXTENDED|REG_NEWLINE)) )
    printf("ERROR %d compiling regex rex_empty\n",rex_err);
  if ( (rex_err = regcomp(&rex_comment,"^[[:blank:]]*#",REG_EXTENDED|REG_NEWLINE)) )
    printf("ERROR %d compiling regex rex_comment\n",rex_err);
  if ( (rex_err = regcomp(&rex_setting,"^[[:blank:]]*([[:alnum:]_]+)[[:blank:]]+([[:graph:]]+)[[:blank:]]*$",REG_EXTENDED|REG_NEWLINE)) )
    printf("ERROR %d compiling regex rex_setting\n",rex_err);
  if ( (rex_err = regcomp(&rex_chsetting,"^[[:blank:]]*([[:alnum:]_]+)[[:blank:]]+([[:digit:]]+)[[:blank:]]+([[:graph:]]+)[[:blank:]]*$",REG_EXTENDED|REG_NEWLINE)) )
    printf("ERROR %d compiling regex rex_chsetting\n",rex_err);

  // See if a configuration file was given
  if (strcmp(cfgfile,"")==0) {
    printf("read_config - No config file specified. Will use defaults.\n");
    return 0;
  }

  // Read configuration from file
  printf("\n=== Reading configuration from file %s ===\n",cfgfile);
  if ( strlen(cfgfile)>=MAX_FILE_LEN ) {
    printf("ERROR - Configuration file name too long (%u characters): %s\n",strlen(cfgfile),cfgfile);
    return 1;
  }
  strcpy(Config->config_file,cfgfile);
  if( (fin = fopen(Config->config_file,"r")) == NULL) {
    printf("ERROR - Cannot open configuration file %s for reading\n",Config->config_file);
    return 1;
  }
  while( (lsize=getline(&line,&len,fin)) != -1 ) {

    if ( line[lsize-1] == '\n' ) line[--lsize] = '\0'; // Remove newline
    //printf("Read line '%s'\n",line);

    // Ignore blank and comment (#) lines
    if ( regexec(&rex_empty,line,0,NULL,0) == 0 ) continue;
    if ( regexec(&rex_comment,line,0,NULL,0) == 0 ) continue;

    // Global parameters
    if ( regexec(&rex_setting,line,nm,rm,0) == 0 ) {

      // Extract parameter name
      plen = rm[1].rm_eo-rm[1].rm_so;
      if (plen>=MAX_PARAM_NAME_LEN) {
	printf("WARNING - Parameter too long (%u characters) in line:\n%s\n",plen,line);
      } else {
	strncpy(param,line+rm[1].rm_so,plen);
	param[plen] = '\0';
      }

      // Extract parameter value
      vlen = rm[2].rm_eo-rm[2].rm_so;
      if (vlen>=MAX_PARAM_VALUE_LEN) {
	printf("WARNING - Value too long (%u characters) in line:\n%s\n",vlen,line);
      } else {
	strncpy(value,line+rm[2].rm_so,vlen);
	value[vlen] = '\0';
      }

      //printf("Parameter <%s> Value <%s>\n",param,value);

      // Interpret parameter
      if ( strcmp(param,"process_id")==0 ) {
	if ( sscanf(value,"%d",&v) ) {
	  Config->process_id = v;
	  printf("Parameter %s set to %d\n",param,v);
	} else {
	  printf("WARNING - Could not parse value %s to number in line:\n%s\n",value,line);
	}
      } else if ( strcmp(param,"process_mode")==0 ) {
	if ( strcmp(value,"DAQ")==0 || strcmp(value,"ZSUP")==0 || strcmp(value,"FAKE")==0) {
	  strcpy(Config->process_mode,value);
	  printf("Parameter %s set to '%s'\n",param,value);
	} else {
	  printf("WARNING - Unknown process mode '%s' selected: ignoring\n",value);
	}
      } else if ( strcmp(param,"start_file")==0 ) {
	if ( strlen(value)<MAX_FILE_LEN ) {
	  strcpy(Config->start_file,value);
	  printf("Parameter %s set to '%s'\n",param,value);
	} else {
	  printf("WARNING - start_file name too long (%u characters): %s\n",strlen(value),value);
	}
      } else if ( strcmp(param,"quit_file")==0 ) {
	if ( strlen(value)<MAX_FILE_LEN ) {
	  strcpy(Config->quit_file,value);
	  printf("Parameter %s set to '%s'\n",param,value);
	} else {
	  printf("WARNING - quit_file name too long (%u characters): %s\n",strlen(value),value);
	}
      } else if ( strcmp(param,"initok_file")==0 ) {
	if ( strlen(value)<MAX_FILE_LEN ) {
	  strcpy(Config->initok_file,value);
	  printf("Parameter %s set to '%s'\n",param,value);
	} else {
	  printf("WARNING - initok_file name too long (%u characters): %s\n",strlen(value),value);
	}
      } else if ( strcmp(param,"initfail_file")==0 ) {
	if ( strlen(value)<MAX_FILE_LEN ) {
	  strcpy(Config->initfail_file,value);
	  printf("Parameter %s set to '%s'\n",param,value);
	} else {
	  printf("WARNING - initfail_file name too long (%u characters): %s\n",strlen(value),value);
	}
      } else if ( strcmp(param,"lock_file")==0 ) {
	if ( strlen(value)<MAX_FILE_LEN ) {
	  strcpy(Config->lock_file,value);
	  printf("Parameter %s set to '%s'\n",param,value);
	} else {
	  printf("WARNING - lock_file name too long (%u characters): %s\n",strlen(value),value);
	}
      } else if ( strcmp(param,"run_number")==0 ) {
	if ( sscanf(value,"%d",&v) ) {
	  Config->run_number = v;
	  printf("Parameter %s set to %d\n",param,v);
	} else {
	  printf("WARNING - Could not parse value %s to number in line:\n%s\n",value,line);
	}
      } else if ( strcmp(param,"input_stream")==0 ) {
	if ( strlen(value)<MAX_DATA_FILE_LEN ) {
	  strcpy(Config->input_stream,value);
	  printf("Parameter %s set to '%s'\n",param,value);
	} else {
	  printf("WARNING - input_stream name too long (%u characters): %s\n",strlen(value),value);
	}
      } else if ( strcmp(param,"output_mode")==0 ) {
	if ( strcmp(value,"FILE")==0 || strcmp(value,"STREAM")==0 ) {
	  strcpy(Config->output_mode,value);
	  printf("Parameter %s set to '%s'\n",param,value);
	} else {
	  printf("WARNING - Unknown output mode '%s' selected: ignoring\n",value);
	}
      } else if ( strcmp(param,"output_stream")==0 ) {
	if ( strlen(value)<MAX_DATA_FILE_LEN ) {
	  strcpy(Config->output_stream,value);
	  printf("Parameter %s set to '%s'\n",param,value);
	} else {
	  printf("WARNING - output_stream name too long (%u characters): %s\n",strlen(value),value);
	}
      } else if ( strcmp(param,"data_dir")==0 ) {
	if ( strlen(value)<MAX_DATA_DIR_LEN ) {
	  strcpy(Config->data_dir,value);
	  printf("Parameter %s set to '%s'\n",param,value);
	} else {
	  printf("WARNING - data_dir name too long (%u characters): %s\n",strlen(value),value);
	}
      } else if ( strcmp(param,"data_file")==0 ) {
	if ( strlen(value)<MAX_DATA_FILE_LEN ) {
	  strcpy(Config->data_file,value);
	  printf("Parameter %s set to '%s'\n",param,value);
	} else {
	  printf("WARNING - data_file name too long (%u characters): %s\n",strlen(value),value);
	}
      } else if ( strcmp(param,"total_daq_time")==0 ) {
	if ( sscanf(value,"%d",&v) ) {
	  Config->total_daq_time = v;
	  printf("Parameter %s set to %d\n",param,v);
	} else {
	  printf("WARNING - Could not parse value %s to number in line:\n%s\n",value,line);
	}
      } else if ( strcmp(param,"board_id")==0 ) {
	if ( sscanf(value,"%d",&v) ) {
	  if (v<MAX_N_BOARDS) {
	    Config->board_id = v;
	    printf("Parameter %s set to %d\n",param,v);
	  } else {
	    printf("WARNING - board_id set to %d, must be < %d\n",v,MAX_N_BOARDS);
	  }
	} else {
	  printf("WARNING - Could not parse value %s to number in line:\n%s\n",value,line);
	}
      } else if ( strcmp(param,"conet2_link")==0 ) {
	if ( sscanf(value,"%d",&v) ) {
	  if (v<MAX_N_CONET2_LINKS) {
	    Config->conet2_link = v;
	    printf("Parameter %s set to %d\n",param,v);
	  } else {
	    printf("WARNING - conet2_link set to %d, must be < %d\n",v,MAX_N_CONET2_LINKS);
	  }
	} else {
	  printf("WARNING - Could not parse value %s to number in line:\n%s\n",value,line);
	}
      } else if ( strcmp(param,"conet2_slot")==0 ) {
	if ( sscanf(value,"%d",&v) ) {
	  if (v<MAX_N_CONET2_SLOTS) {
	    Config->conet2_slot = v;
	    printf("Parameter %s set to %d\n",param,v);
	  } else {
	    printf("WARNING - conet2_slot set to %d, must be < %d\n",v,MAX_N_CONET2_SLOTS);
	  }
	} else {
	  printf("WARNING - Could not parse value %s to number in line:\n%s\n",value,line);
	}
      } else if ( strcmp(param,"startdaq_mode")==0 ) {
	if ( sscanf(value,"%d",&v) ) {
	  if ( v>2 ) {
	    printf("WARNING - Invalid value for startdaq_mode: %d. Accepted: 0,1,2\n",v);
	  } else {
	    Config->startdaq_mode = v;
	    printf("Parameter %s set to %d\n",param,v);
	  }
	} else {
	  printf("WARNING - Could not parse value %s to number in line:\n%s\n",value,line);
	}
      } else if ( strcmp(param,"drs4_sampfreq")==0 ) {
	if ( sscanf(value,"%d",&v) ) {
	  if ( v>2 ) {
	    printf("WARNING - Invalid value for drs4_sampfreq: %d. Accepted: 0,1,2\n",v);
	  } else {
	    Config->drs4_sampfreq = v;
	    printf("Parameter %s set to %d\n",param,v);
	  }
	} else {
	  printf("WARNING - Could not parse value %s to number in line:\n%s\n",value,line);
	}
      } else if ( strcmp(param,"trigger_mode")==0 ) {
	if ( sscanf(value,"%d",&v) ) {
	  if ( v>2 ) {
	    printf("WARNING - Invalid value for trigger_mode: %d. Accepted: 0,1,2\n",v);
	  } else {
	    Config->trigger_mode = v;
	    printf("Parameter %s set to %d\n",param,v);
	  }
	} else {
	  printf("WARNING - Could not parse value %s to number in line:\n%s\n",value,line);
	}
      } else if ( strcmp(param,"trigger_iolevel")==0 ) {
	if ( strcmp(value,"NIM")==0 || strcmp(value,"TTL")==0 ) {
	  strcpy(Config->trigger_iolevel,value);
	  printf("Parameter %s set to '%s'\n",param,value);
	} else {
	  printf("WARNING - Value %s not valid for parameter %s: use NIM or TTL\n",value,param);
	}
      } else if ( strcmp(param,"group_enable_mask")==0 ) {
	if ( sscanf(value,"%x",&vu) ) {
	  Config->group_enable_mask = vu;
	  printf("Parameter %s set to 0x%1x\n",param,vu);
	} else {
	  printf("WARNING - Could not parse value %s to number in line:\n%s\n",value,line);
	}
      } else if ( strcmp(param,"channel_enable_mask")==0 ) {
	if ( sscanf(value,"%x",&vu) ) {
	  Config->channel_enable_mask = vu;
	  printf("Parameter %s set to 0x%08x\n",param,vu);
	} else {
	  printf("WARNING - Could not parse value %s to number in line:\n%s\n",value,line);
	}
      } else if ( strcmp(param,"offset_global")==0 ) {
	if ( sscanf(value,"%x",&vu) ) {
	  Config->offset_global = vu;
	  for(ch=0;ch<32;ch++) Config->offset_ch[ch] = Config->offset_global;
	  printf("Parameter %s set to 0x%04x\n",param,vu);
	} else {
	  printf("WARNING - Could not parse value %s to number in line:\n%s\n",value,line);
	}
      } else if ( strcmp(param,"post_trigger_size")==0 ) {
	if ( sscanf(value,"%u",&vu) ) {
	  Config->post_trigger_size = vu;
	  printf("Parameter %s set to %d\n",param,vu);
	} else {
	  printf("WARNING - Could not parse value %s to number in line:\n%s\n",value,line);
	}
      } else if ( strcmp(param,"max_num_events_blt")==0 ) {
	if ( sscanf(value,"%u",&vu) ) {
	  Config->max_num_events_blt = vu;
	  printf("Parameter %s set to %d\n",param,vu);
	} else {
	  printf("WARNING - Could not parse value %s to number in line:\n%s\n",value,line);
	}
      } else if ( strcmp(param,"drs4corr_enable")==0 ) {
	if ( sscanf(value,"%d",&v) ) {
	  Config->drs4corr_enable = v;
	  printf("Parameter %s set to %d\n",param,v);
	} else {
	  printf("WARNING - Could not parse value %s to number in line:\n%s\n",value,line);
	}
      } else if ( strcmp(param,"daq_loop_delay")==0 ) {
	if ( sscanf(value,"%d",&v) ) {
	  Config->daq_loop_delay = v;
	  printf("Parameter %s set to %d\n",param,v);
	} else {
	  printf("WARNING - Could not parse value %s to number in line:\n%s\n",value,line);
	}
      } else if ( strcmp(param,"zero_suppression")==0 ) {
	if ( sscanf(value,"%d",&v) ) {
	  Config->zero_suppression = v;
	  printf("Parameter %s set to %d\n",param,v);
	} else {
	  printf("WARNING - Could not parse value %s to number in line:\n%s\n",value,line);
	}
      } else if ( strcmp(param,"zs1_head")==0 ) {
	if ( sscanf(value,"%d",&v) ) {
	  Config->zs1_head = v;
	  printf("Parameter %s set to %d\n",param,v);
	} else {
	  printf("WARNING - Could not parse value %s to number in line:\n%s\n",value,line);
	}
      } else if ( strcmp(param,"zs1_tail")==0 ) {
	if ( sscanf(value,"%d",&v) ) {
	  Config->zs1_tail = v;
	  printf("Parameter %s set to %d\n",param,v);
	} else {
	  printf("WARNING - Could not parse value %s to number in line:\n%s\n",value,line);
	}
      } else if ( strcmp(param,"zs1_nsigma")==0 ) {
	if ( sscanf(value,"%f",&vf) ) {
	  Config->zs1_nsigma = vf;
	  printf("Parameter %s set to %f\n",param,vf);
	} else {
	  printf("WARNING - Could not parse value %s to number in line:\n%s\n",value,line);
	}
      } else if ( strcmp(param,"zs1_nabovethr")==0 ) {
	if ( sscanf(value,"%d",&v) ) {
	  Config->zs1_nabovethr = v;
	  printf("Parameter %s set to %d\n",param,v);
	} else {
	  printf("WARNING - Could not parse value %s to number in line:\n%s\n",value,line);
	}
      } else if ( strcmp(param,"zs1_badrmsthr")==0 ) {
	if ( sscanf(value,"%f",&vf) ) {
	  Config->zs1_badrmsthr = vf;
	  printf("Parameter %s set to %f\n",param,vf);
	} else {
	  printf("WARNING - Could not parse value %s to number in line:\n%s\n",value,line);
	}
      } else if ( strcmp(param,"zs2_tail")==0 ) {
	if ( sscanf(value,"%d",&v) ) {
	  Config->zs2_tail = v;
	  printf("Parameter %s set to %d\n",param,v);
	} else {
	  printf("WARNING - Could not parse value %s to number in line:\n%s\n",value,line);
	}
      } else if ( strcmp(param,"zs2_minrms")==0 ) {
	if ( sscanf(value,"%f",&vf) ) {
	  Config->zs2_minrms = vf;
	  for(ch=0;ch<32;ch++) Config->zs2_minrms_ch[ch] = Config->zs2_minrms;
	  printf("Parameter %s set to %f\n",param,vf);
	} else {
	  printf("WARNING - Could not parse value %s to number in line:\n%s\n",value,line);
	}
      } else if ( strcmp(param,"auto_threshold")==0 ) {
	if ( sscanf(value,"%x",&vu) ) {
	  Config->auto_threshold = vu;
	  printf("Parameter %s set to 0x%04x\n",param,vu);
	} else {
	  printf("WARNING - Could not parse value %s to number in line:\n%s\n",value,line);
	}
      } else if ( strcmp(param,"auto_duration")==0 ) {
	if ( sscanf(value,"%u",&vu) ) {
	  Config->auto_duration = vu;
	  printf("Parameter %s set to %u\n",param,vu);
	} else {
	  printf("WARNING - Could not parse value %s to number in line:\n%s\n",value,line);
	}
      } else if ( strcmp(param,"file_max_duration")==0 ) {
	if ( sscanf(value,"%d",&v) ) {
	  Config->file_max_duration = v;
	  printf("Parameter %s set to %d\n",param,v);
	} else {
	  printf("WARNING - Could not parse value %s to number in line:\n%s\n",value,line);
	}
      } else if ( strcmp(param,"file_max_size")==0 ) {
	if ( sscanf(value,"%llu",&vul) ) {
	  Config->file_max_size = vul;
	  printf("Parameter %s set to %llu\n",param,vul);
	} else {
	  printf("WARNING - Could not parse value %s to number in line:\n%s\n",value,line);
	}
      } else if ( strcmp(param,"file_max_events")==0 ) {
	if ( sscanf(value,"%u",&vu) ) {
	  Config->file_max_events = vu;
	  printf("Parameter %s set to %d\n",param,vu);
	} else {
	  printf("WARNING - Could not parse value %s to number in line:\n%s\n",value,line);
	}
      } else if ( strcmp(param,"debug_scale")==0 ) {
        if ( sscanf(value,"%u",&vu) ) {
          Config->debug_scale = vu;
          printf("Parameter %s set to %d\n",param,vu);
        } else {
          printf("WARNING - Could not parse value %s to number in line:\n%s\n",value,line);
        }
      } else {
	printf("WARNING - Unknown parameter %s from line:\n%s\n",param,line);
      }

    // Channel parameters
    } else if ( regexec(&rex_chsetting,line,ncm,rcm,0) == 0 ) {

      // Extract parameter name
      plen = rcm[1].rm_eo-rcm[1].rm_so;
      if (plen>=MAX_PARAM_NAME_LEN) {
	printf("WARNING - Parameter too long (%u characters) in line:\n%s\n",plen,line);
      } else {
	strncpy(param,line+rcm[1].rm_so,plen);
	param[plen] = '\0';
      }

      // Extract channel number
      ch = -1;
      vlen = rcm[2].rm_eo-rcm[2].rm_so;
      if (vlen>=3) {
	printf("WARNING - Character number too long (%u characters) in line:\n%s\n",vlen,line);
      } else {
	strncpy(value,line+rcm[2].rm_so,vlen);
	value[vlen] = '\0';
	if ( sscanf(value,"%d",&ch) ) {
	  if (ch>=32) printf("WARNING - Wrong channel number %d in line:\n%s\n",ch,line);
	} else {
	  printf("WARNING - Could not parse value %s to channel number in line:\n%s\n",value,line);
	}
      }
      if (ch<0 || ch>31) {
	printf("WARNING - Invalid channel number %d in line:\n%s\n",ch,line);
	continue;
      }

      // Extract parameter value
      vlen = rcm[3].rm_eo-rcm[3].rm_so;
      if (vlen>=MAX_PARAM_VALUE_LEN) {
	printf("WARNING - Value too long (%u characters) in line:\n%s\n",vlen,line);
      } else {
	strncpy(value,line+rcm[3].rm_so,vlen);
	value[vlen] = '\0';
      }

      //printf("Parameter <%s> Channel <%d> Value <%s>\n",param,ch,value);

      if ( strcmp(param,"offset_ch")==0 ) {
	if ( sscanf(value,"%x",&v) ) {
	  Config->offset_ch[ch] = v;
	  printf("Parameter %s for channel %d set to 0x%04x\n",param,ch,v);
	} else {
	  printf("WARNING - Could not parse value %s to number in line:\n%s\n",value,line);
	}
      } else if ( strcmp(param,"zs2_minrms_ch")==0 ) {
	if ( sscanf(value,"%f",&vf) ) {
	  Config->zs2_minrms_ch[ch] = vf;
	  printf("Parameter %s for channel %d set to %f\n",param,ch,vf);
	} else {
	  printf("WARNING - Could not parse value %s to number in line:\n%s\n",value,line);
	}
      } else {
	printf("WARNING - Unknown channel parameter %s from line:\n%s\n",param,line);
      }

    } else {
      printf("WARNING - Could not parse line in confg file:\n%s\n",line);
    }

  }

  free(line);
  fclose(fin);

  // Release memory used be regular expressions
  regfree(&rex_empty);
  regfree(&rex_comment);
  regfree(&rex_setting);
  regfree(&rex_chsetting);

  return 0;
}

int print_config(){

  int i;

  printf("\n=== Configuration parameters for this run ===\n");
  printf("process_id\t\t%d\t\tDB id for this process\n",Config->process_id);
  printf("process_mode\t\t'%s'\t\tfunctioning mode for this PadmeDAQ process (DAQ, ZSUP, or FAKE)\n",Config->process_mode);
  printf("config_file\t\t'%s'\tname of configuration file (can be empty)\n",Config->config_file);

  // Control files are only used by DAQ. Will disappear when HW run control signals will be in place
  if (strcmp(Config->process_mode,"DAQ")==0) {
    printf("start_file\t\t'%s'\tname of start file. DAQ will start when this file is created\n",Config->start_file);
    printf("quit_file\t\t'%s'\tname of quit file. DAQ will exit when this file is created\n",Config->quit_file);
  }

  printf("initok_file\t\t'%s'\tname of initok file. Created when board is correctly initialized and ready fo DAQ\n",Config->initok_file);
  printf("initfail_file\t\t'%s'\tname of initfail file. Created when board initialization failed\n",Config->initfail_file);
  printf("lock_file\t\t'%s'\tname of lock file. Contains PID of locking process\n",Config->lock_file);

  printf("run_number\t\t%d\t\trun number (0: dummy run)\n",Config->run_number);

  // Only ZSUP uses STREAM input
  if (strcmp(Config->process_mode,"ZSUP")==0) {
    printf("input_stream\t\t'%s'\tname of virtual file used as input stream\n",Config->input_stream);
  }

  printf("output_mode\t\t%s\t\toutput mode (FILE or STREAM)\n",Config->output_mode);
  if (strcmp(Config->output_mode,"STREAM")==0) {
    printf("output_stream\t\t%s\t\tname of virtual file used as output stream\n",Config->output_stream);
  } else {
    printf("data_dir\t\t'%s'\t\tdirectory where output files will be stored\n",Config->data_dir);
    printf("data_file\t\t'%s'\ttemplate name for data files: <date/time> string will be appended\n",Config->data_file);
  }

  printf("board_id\t\t%d\t\tboard ID\n",Config->board_id);
  
  printf("conect_mode\t\t%s\t\tADC module connection mode (USB or OPTICAL)\n",Config->connect_mode);
  printf("conet2_link\t\t%d\t\tCONET2 link\n",Config->conet2_link);
  printf("conet2_slot\t\t%d\t\tCONET2 slot\n",Config->conet2_slot);

  // Show parameters which are relevant for DAQ or FAKE (N.B. FAKE only uses a subset of them)
  if (strcmp(Config->process_mode,"DAQ")==0 || strcmp(Config->process_mode,"FAKE")==0) {
    printf("total_daq_time\t\t%d\t\ttime (secs) after which daq will stop. 0=run forever\n",Config->total_daq_time);
    printf("startdaq_mode\t\t%d\t\tstart/stop daq mode (0:SW, 1:S_IN, 2:trg)\n",Config->startdaq_mode);
    printf("drs4_sampfreq\t\t%d\t\tDRS4 sampling frequency (0:5GHz, 1:2.5GHz, 2:1GHz)\n",Config->drs4_sampfreq);
    printf("trigger_mode\t\t%d\t\ttrigger mode (0:ext, 1:fast, 2:sw)\n",Config->trigger_mode);
    printf("trigger_iolevel\t\t'%s'\t\ttrigger signal IO level (NIM or TTL)\n",Config->trigger_iolevel);
    printf("group_enable_mask\t0x%1x\t\tmask to enable groups of channels\n",Config->group_enable_mask);
    printf("channel_enable_mask\t0x%08x\tmask to enable individual channels\n",Config->channel_enable_mask);
    printf("offset_global\t\t0x%04x\t\tglobal DC offset\n",Config->offset_global);
    for(i=0;i<32;i++) {
      if (Config->offset_ch[i] != Config->offset_global) printf("offset_ch\t%.2d\t0x%04x\n",i,Config->offset_ch[i]);
    }
    printf("post_trigger_size\t%d\t\tpost trigger size\n",Config->post_trigger_size);
    printf("max_num_events_blt\t%d\t\tmax number of events to transfer in a single readout\n",Config->max_num_events_blt);
    printf("drs4corr_enable\t\t%d\t\tenable (1) or disable (0) DRS4 corrections to sampled data\n",Config->drs4corr_enable);
    printf("daq_loop_delay\t\t%d\t\twait time inside daq loop in usecs\n",Config->daq_loop_delay);
    printf("auto_threshold\t\t0x%04x\t\tautopass: threshold below which trigger is considered ON\n",Config->auto_threshold);
    printf("auto_duration\t\t%d\t\tautopass: number of ns of trigger ON above which autopass is enabled\n",Config->auto_duration);
  }

  // Show parameters which are relevant for ZSUP
  if (strcmp(Config->process_mode,"ZSUP")==0) {
    printf("zero_suppression\t%d\t\tzero-suppression - 100*mode+algorithm (mode:0=reject,1=flag - algorithm:0=OFF,1-15=algorithm id)\n",Config->zero_suppression);

    // Only show parameters which are relevant for the selected zero suppression algorithm
    if (Config->zero_suppression%100 == 1) {
      printf("zs1_head\t\t%d\t\tnumber of samples to use to compute mean and rms\n",Config->zs1_head);
      printf("zs1_tail\t\t%d\t\tnumber of samples to reject at the end\n",Config->zs1_tail);
      printf("zs1_nsigma\t\t%5.3f\t\tnumber of sigmas around mean used to set the threshold\n",Config->zs1_nsigma);
      printf("zs1_nabovethr\t\t%d\t\tnumber of consecutive above-threshold samples required to accept the channel\n",Config->zs1_nabovethr);
      printf("zs1_badrmsthr\t\t%5.1f\t\trms value above which channel is accepted as problematic\n",Config->zs1_badrmsthr);
    } else if (Config->zero_suppression%100 == 2) {
      printf("zs2_tail\t\t%d\t\tnumber of samples to reject at the end\n",Config->zs2_tail);
      printf("zs2_minrms\t\t%8.3f\tglobal RMS threshold to accept the event\n",Config->zs2_minrms);
    for(i=0;i<32;i++) {
      if (Config->zs2_minrms_ch[i] != Config->zs2_minrms) printf("zs2_minrms_ch\t%.2d\t%8.3f\tRMS threshold for channel %d\n",i,Config->zs2_minrms_ch[i],i);
    }
    }
  }

  // These are only relevant for FILE output mode as in STREAM mode the output file never changes
  if (strcmp(Config->output_mode,"FILE")==0) {
    printf("file_max_duration\t%d\t\tmax time to write data before changing output file\n",Config->file_max_duration);
    printf("file_max_size\t\t%llu\tmax size of output file before changing it\n",Config->file_max_size);
    printf("file_max_events\t\t%u\t\tmax number of events to write before changing output file\n",Config->file_max_events);
  }

  printf("debug_scale\t\t%u\t\tDebug output downscale factor\n",Config->debug_scale);

  printf("=== End of configuration parameters ===\n\n");

  return 0;

}

int end_config()
{
  free(Config);
  return 0;
}
