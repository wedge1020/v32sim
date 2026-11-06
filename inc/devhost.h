#ifndef  _DEVHOST_H
#define  _DEVHOST_H

////////////////////////////////////////////////////////////////////////////////////////
//
// devhost.h: a native, in-process v32dev host (storage and serial devices)
//
// --v32dev-host=N=SPEC serves channel N of the memory card's v32dev mailbox from inside
// the simulator, with the behaviour of tools/v32dev.py of the v32io project (the
// reference): the same specs, descriptors, opcodes, status codes and ring rules. The
// host works directly on the card's memory, so it needs no file round trip and, with
// --v32dev-doorbell, answers a ring at once and without any wall clock: runs are
// repeatable. The card's file still receives everything the program writes.
//
//   SPEC   storage:FILE[,bytes][,ro][,sector=N][,create=SECTORS]
//          serial:stdio | serial:echo[,hup=N]
//          serial: also takes, in any combination (v32sim extensions):
//                  in=FILE (fed to the program at start and at every reopen of the channel),
//                  out=FILE (low 8 bits of what the program writes), hangup (hang up once
//                  all the input has been delivered)
//
////////////////////////////////////////////////////////////////////////////////////////

int       devhost_add         (const char *);              // --v32dev-host=N=SPEC; 0 = accepted
uint8_t   devhost_configured  (void);                      // some channel was asked for
uint8_t   devhost_ok          (void);                      // ...and all of them are being served
uint8_t   devhost_serves      (int);                       // is channel N served natively?
void      devhost_stdin       (uint8_t);                   // may serial:stdio read standard input?
int       devhost_types       (uint32_t *);                // channel types for formatting; returns how many are served
void      devhost_start       (void);                      // the mailbox is live: open the devices and announce them
void      devhost_fail        (void);                      // formatting the card failed
void      devhost_stop        (void);                      // withdraw, close
void      devhost_resync      (void);                      // card memory was replaced (savestate load)
void      devhost_frame       (void);                      // frame boundary: heartbeat and work for every channel
void      devhost_poll        (int);                       // work for one channel (a doorbell ring)
void      devhost_describe    (int, char *, size_t);       // one line for the 'mailbox' command
extern uint8_t   devhost_force;                                   // --v32dev-host-force: format a card with other data in its mailbox region

#endif
