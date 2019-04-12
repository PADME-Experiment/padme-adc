#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <libgen.h>

#include "Config.h"
#include "Tools.h"
#include "DAQ.h"
#include "ZSUP.h"
#include "FAKE.h"

// Start of main program
int main(int argc, char*argv[])
{

  pid_t pid;
  int c;
  int rc;

  // Make sure local data types are correct for us
  if (sizeof(int)<4) {
    printf("*** On this system sizeof(int) is %u bytes while we need at least 4 bytes. Aborting ***\n",sizeof(int));
    exit(1);
  }
  if (sizeof(long long)<8) {
    printf("*** On this system sizeof(long long) is %u bytes while we need at least 8 bytes. Aborting ***\n",sizeof(long long));
    exit(1);
  }

  // Use line buffering for stdout
  setlinebuf(stdout);

  // Show welcome message
  printf("=======================================\n");
  printf("=== Welcome to the PADME ADC system ===\n");
  printf("=======================================\n");

  // Initialize run configuration
  if ( init_config() ) {
    printf("*** ERROR *** Problem initializing run configuration.\n");
    exit(1);
  }

  // Parse options
  while ((c = getopt (argc, argv, "c:h")) != -1)
    switch (c)
      {
      case 'c':
	// Check if another configuration file was specified
	if ( strcmp(Config->config_file,"")!=0 ) {
	  printf("*** ERROR *** Multiple configuration files specified: '%s' and '%s'.\n",Config->config_file,optarg);
	  exit(1);
	}
	// Read configuration parameters from configuration file
	if ( read_config(optarg) ) {
	  printf("*** ERROR *** Problem while reading configuration file '%s'.\n",optarg);
	  exit(1);
	}
        break;
      case 'h':
	fprintf(stdout,"\nPadmeADC [-c cfg_file] [-h]\n\n");
	fprintf(stdout,"  -c: use file 'cfg_file' to set configuration parameters for this process\n");
	fprintf(stdout,"     If no file is specified, use default settings\n");
	fprintf(stdout,"  -h: show this help message and exit\n\n");
	exit(0);
      case '?':
        if (optopt == 'c')
          fprintf (stderr, "Option -%c requires an argument.\n", optopt);
        else if (isprint (optopt))
          fprintf (stderr, "Unknown option '-%c'.\n", optopt);
        else
          fprintf (stderr,"Unknown option character '\\x%x'.\n",optopt);
        exit(1);
      default:
        abort ();
      }

  // Show configuration
  print_config();

  // Check if another PadmeDAQ program is running
  printf("\n=== Verifying that no other PadmeADC instances are running ===\n");
  if ( (pid = create_lock()) ) {
    if (pid > 0) {
      printf("*** ERROR *** Another PadmeADC is running with PID %d. Exiting.\n",pid);
    } else {
      printf("*** ERROR *** Problems while creating lock file '%s'. Exiting.\n",Config->lock_file);
    }
    create_initfail_file();
    exit(1);
  }

  // Check current running mode (DAQ, ZSUP, FAKE)

  if ( strcmp(Config->process_mode,"DAQ")==0 ) {

    // Show some startup message
    if (Config->run_number == 0) {
      printf("\n=== Starting PadmeADC acquisition for dummy run ===\n");
    } else {
      printf("\n=== Starting PadmeADC acquisition for run %d ===\n",Config->run_number);
    }

    // Connect to digitizer
    printf("\n=== Connect to digitizer ===\n");
    if ( DAQ_connect() ) {
      printf("*** ERROR *** Problem while connecting to digitizer. Exiting.\n");
      create_initfail_file();
      remove_lock();
      exit(1);
    }

    // Initialize and configure digitizer
    printf("\n=== Initialize digitizer ===\n");
    if ( DAQ_init() ) {
      printf("*** ERROR *** Problem while initializing digitizer module. Exiting.\n");
      create_initfail_file();
      remove_lock();
      exit(1);
    }

    // Handle data acquisition
    printf("\n=== Starting data acquisition ===\n");
    rc = DAQ_readdata();
    if ( rc == 0 ) {
      printf("=== Run finished ===\n");
    } else if ( rc == 1 ) {
      printf("*** ERROR *** Problem while initializing DAQ process. Exiting.\n");
      create_initfail_file();
      remove_lock();
      exit(1);
    } else if ( rc == 2 ) {
      printf("*** ERROR *** DAQ ended with an error. Please check log file for details. Exiting.\n");
      remove_lock();
      exit(1);
    } else if ( rc == 3 ) {
      printf("=== Run aborted before starting DAQ ===\n");
    } else {
      printf("=== DAQ reported unknown return code %d ===\n",rc);
    }

    // Final reset of the digitizer
    printf("\n=== Reset digitizer and close connection ===\n");
    if ( DAQ_close() ) {
      printf("*** ERROR *** Final reset of digitizer ended with an error. Exiting.\n");
      remove_lock();
      exit(1);
    }

  } else if ( strcmp(Config->process_mode,"FAKE")==0 ) {

    /*
    // Show some startup message
    if (Config->run_number == 0) {
      printf("\n=== Starting PadmeADC FAKE event generation for dummy run ===\n");
    } else {
      printf("\n*** ERROR *** Cannot run PadmeADC in FAKE mode for a real run. Exiting.\n");
      create_initfail_file();
      remove_lock();
      exit(1);
    }

    // Start generation of FAKE events
    rc = FAKE_readdata();
    if ( rc == 1 ) {
      printf("*** ERROR *** Problem while initializing FAKE process. Exiting.\n");
      create_initfail_file();
      remove_lock();
      exit(1);
    } else if ( rc == 2 ) {
      printf("*** ERROR *** FAKE event generation ended with an error. Please check log file for details. Exiting.\n");
      remove_lock();
      exit(1);
    }

    printf("\n=== FAKE event generation process ended ===\n");
    */
    printf("\n=== FAKE event generation currently not supported ===\n");

  } else if ( strcmp(Config->process_mode,"ZSUP")==0 ) {

    /*
    // Show some startup message
    if (Config->run_number == 0) {
      printf("\n=== Starting PadmeDAQ zero suppression for dummy run ===\n");
    } else {
      printf("\n=== Starting PadmeDAQ zero suppression for run %d ===\n",Config->run_number);
    }

    // Handle zero suppression
    printf("\n=== Starting zero suppression ===\n");
    rc = ZSUP_readdata();
    if ( rc == 0 ) {
      printf("\n=== ZSUP process ended ===\n");
    } else if ( rc == 1 ) {
      printf("*** ERROR *** Problem while initializing ZSUP process. Exiting.\n");
      create_initfail_file();
      remove_lock();
      exit(1);
    } else if ( rc == 2 ) {
      printf("*** ERROR *** Zero suppression ended with an error. Please check log file for details. Exiting.\n");
      remove_lock();
      exit(1);
    } else if ( rc == 3 ) {
      printf("=== Run aborted before starting ZSUP ===\n");
    } else {
      printf("=== ZSUP reported unknown return code %d ===\n",rc);
    }
    */
    printf("\n=== ZSUP zero suppression currently not supported ===\n");

  }

  // Remove lock file
  remove_lock();

  // Clean up configuration
  end_config();

  // All is done: exit
  printf("\n=== May the force be with you. Bye! ===\n");
  exit(0);

}
