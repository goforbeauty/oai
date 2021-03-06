// ===========================================================================
#include <stdio.h>
#include <signal.h>
// ===========================================================================
#include "RRCMessageHandler.h"
// ===========================================================================
static	bool quit = false;
// ===========================================================================
void sigint(int)
// ===========================================================================
{
  quit = true;
}
// ===========================================================================
int main(int argc,char *argv[])
// ===========================================================================
{

  signal(SIGPIPE, SIG_IGN);
  signal(SIGINT, sigint);

  // start RRC server
  RRCMessageHandler::Instance();
  RRCMessageHandler::Instance()->Join(&quit);
  fprintf(stderr, "\nRRM LTE Exiting...\n");
  return 0;
}


