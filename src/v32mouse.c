#include "defines.h"

////////////////////////////////////////////////////////////////////////////////////////
//
// v32mouse.c: the v32mouse mouse on a gamepad port (see v32io.h)
//
// The device keeps two counters of 12 positions, one per axis, and shows them on the
// 11 controls of its gamepad (lib/v32io/README.md of the v32io project):
//
//   X: Left = trit -, Right = trit +, X = Gray high, Y = Gray low
//   Y: Up   = trit -, Down  = trit +, L = Gray high, R = Gray low
//   buttons: Start = middle, A = left, B = right
//
// A position is a Gray code (4 groups of 3) and a trit that runs - 0 + in even groups
// and + 0 - in odd ones, so every step changes exactly one control. A mouse at rest
// shows nothing pressed (both counters at position 1).
//
// Actions wait in a queue and are delivered one per frame, as a device that is probed
// once per frame sees them: a move of up to 5 steps per axis, or one button change, or
// a frame of waiting. A longer move takes several frames. This is the order and timing
// of tools/v32io-encode.py of the v32io project, and tests/vectors/ checks both.
//
////////////////////////////////////////////////////////////////////////////////////////

#define  MQ_MAX      4096
#define  M_MOVE      0
#define  M_DOWN      1
#define  M_UP        2
#define  M_WAIT      3
#define  M_STEPS     5                     // most steps per axis in one frame
#define  M_POSITIONS 12

#define  B_MIDDLE    1
#define  B_LEFT      2
#define  B_RIGHT     4

typedef struct
{
    uint8_t  op;
    int32_t  a;                            // MOVE: dx   DOWN/UP: button bit   WAIT: frames left
    int32_t  b;                            // MOVE: dy
} mact_t;

static struct
{
    int       port;                        // gamepad port of the device, -1 = none
    int       cx;                          // counter positions, 0-11
    int       cy;
    uint8_t   buttons;
    mact_t    q[MQ_MAX];
    int       qhead;
    int       qcount;
    uint32_t  frames;                      // actions delivered
    uint32_t  stepsx;                      // steps delivered, with sign
    uint32_t  stepsy;
    uint32_t  dropped;                     // actions lost because the queue was full
} mouse = { .port = -1, .cx = 1, .cy = 1 };

////////////////////////////////////////////////////////////////////////////////////////
//
// counters <-> controls
//
// position -> (negative, positive, gray high, gray low)
//
static void  counter_controls (int pos, int *neg, int *plus, int *high, int *low)
{
    static const uint8_t  gray[4]  = { 0, 1, 3, 2 };            // 00 01 11 10
    int  group  = pos / 3;
    int  t      = pos % 3;
    int  trit   = ((group & 1) != 0) ? (2 - t) : t;             // 0 negative, 1 none, 2 positive

    *neg   = (trit == 0) ? 1 : 0;
    *plus  = (trit == 2) ? 1 : 0;
    *high  = (gray[group] >> 1) & 1;
    *low   = gray[group] & 1;
}

//
// the position that shows these controls, or -1 for a combination that is not one
//
static int  counter_position (int neg, int plus, int high, int low)
{
    int  pos  = 0;
    int  n    = 0;
    int  p    = 0;
    int  h    = 0;
    int  l    = 0;

    for (pos = 0; pos < M_POSITIONS; pos = pos + 1)
    {
        counter_controls (pos, &n, &p, &h, &l);
        if ((n == neg) && (p == plus) && (h == high) && (l == low))
        {
            return (pos);
        }
    }
    return (-1);
}

//
// present(): put the device's state on its gamepad. A control already in the state
// asked for is left alone, so its frame count keeps running
//
static void  present (void)
{
    uint8_t  pad  = (uint8_t) mouse.port;
    int      n    = 0;
    int      p    = 0;
    int      h    = 0;
    int      l    = 0;

    counter_controls (mouse.cx, &n, &p, &h, &l);
    gamepad_control (pad, 0,  (uint8_t) n);
    gamepad_control (pad, 1,  (uint8_t) p);
    gamepad_control (pad, 7,  (uint8_t) h);
    gamepad_control (pad, 8,  (uint8_t) l);
    counter_controls (mouse.cy, &n, &p, &h, &l);
    gamepad_control (pad, 2,  (uint8_t) n);
    gamepad_control (pad, 3,  (uint8_t) p);
    gamepad_control (pad, 9,  (uint8_t) h);
    gamepad_control (pad, 10, (uint8_t) l);
    gamepad_control (pad, 4,  (uint8_t) ((mouse.buttons & B_MIDDLE) ? 1 : 0));
    gamepad_control (pad, 5,  (uint8_t) ((mouse.buttons & B_LEFT)   ? 1 : 0));
    gamepad_control (pad, 6,  (uint8_t) ((mouse.buttons & B_RIGHT)  ? 1 : 0));
}

//
// resync(): take the counters and buttons from what the gamepad shows (after a state
// was restored, or when the device is attached to a gamepad that already shows some)
//
static void  resync (void)
{
    uint8_t  pad  = (uint8_t) mouse.port;
    int      x    = 0;
    int      y    = 0;

    x  = counter_position (gamepad_pressed (pad, 0), gamepad_pressed (pad, 1),
                           gamepad_pressed (pad, 7), gamepad_pressed (pad, 8));
    y  = counter_position (gamepad_pressed (pad, 2), gamepad_pressed (pad, 3),
                           gamepad_pressed (pad, 9), gamepad_pressed (pad, 10));
    if (x >= 0)  mouse.cx  = x;
    if (y >= 0)  mouse.cy  = y;
    mouse.buttons  = (uint8_t) ((gamepad_pressed (pad, 4) ? B_MIDDLE : 0) |
                                (gamepad_pressed (pad, 5) ? B_LEFT   : 0) |
                                (gamepad_pressed (pad, 6) ? B_RIGHT  : 0));
}

////////////////////////////////////////////////////////////////////////////////////////
//
// the action queue
//
static int  add (uint8_t op, int32_t a, int32_t b)
{
    mact_t  *m  = NULL;

    if (mouse.port < 0)
    {
        fprintf (stderr, "[ERROR] mouse: not attached (try 'mouse attach [PORT]')\n");
        return (-1);
    }
    if (mouse.qcount >= MQ_MAX)
    {
        mouse.dropped  = mouse.dropped + 1;
        fprintf (stderr, "[ERROR] mouse: %d actions are waiting, this one is dropped\n", MQ_MAX);
        return (-1);
    }
    m      = mouse.q + ((mouse.qhead + mouse.qcount) % MQ_MAX);
    m -> op  = op;
    m -> a   = a;
    m -> b   = b;
    mouse.qcount  = mouse.qcount + 1;
    return (0);
}

static int  button_bit (const char *name)
{
    if (0 == strcasecmp (name, "left"))    return (B_LEFT);
    if (0 == strcasecmp (name, "right"))   return (B_RIGHT);
    if (0 == strcasecmp (name, "middle"))  return (B_MIDDLE);
    fprintf (stderr, "[ERROR] mouse: unknown button '%s' (left, right or middle)\n", name);
    return (0);
}

int  v32mouse_move (int dx, int dy)
{
    return ((dx == 0) && (dy == 0)) ? 0 : add (M_MOVE, dx, dy);
}

int  v32mouse_button (const char *name, const char *mode)
{
    int  bit  = button_bit (name);

    if (bit == 0)
    {
        return (-1);
    }
    if ((mode == NULL) || (0 == strcasecmp (mode, "click")))
    {
        // down, two frames, up: a press the program cannot miss
        if ((add (M_DOWN, bit, 0) != 0) || (add (M_WAIT, 2, 0) != 0))  return (-1);
        return (add (M_UP, bit, 0));
    }
    if (0 == strcasecmp (mode, "down"))  return (add (M_DOWN, bit, 0));
    if (0 == strcasecmp (mode, "up"))    return (add (M_UP, bit, 0));
    fprintf (stderr, "[ERROR] mouse: unknown mode '%s' (down, up, click)\n", mode);
    return (-1);
}

int  v32mouse_wait (int frames)
{
    return (frames > 0) ? add (M_WAIT, frames, 0) : 0;
}

void  v32mouse_clear (void)
{
    mouse.qhead   = 0;
    mouse.qcount  = 0;
    if ((mouse.port >= 0) && (gamepad[mouse.port].connected == TRUE))
    {
        resync ();
    }
}

static int  clamp (int v)
{
    return (v > M_STEPS) ? M_STEPS : ((v < -M_STEPS) ? -M_STEPS : v);
}

static int  wrap (int pos)
{
    return (((pos % M_POSITIONS) + M_POSITIONS) % M_POSITIONS);
}

//
// once per frame, before it runs (v32io_frame): deliver the action at the head of the
// queue and show the state
//
void  v32mouse_frame (void)
{
    mact_t  *m   = NULL;
    int      sx  = 0;
    int      sy  = 0;

    if (mouse.port < 0)
    {
        return;
    }
    if (gamepad[mouse.port].connected != TRUE)
    {
        mouse.qhead   = 0;                              // someone unplugged it
        mouse.qcount  = 0;
        return;
    }
    if (mouse.qcount > 0)
    {
        m  = mouse.q + mouse.qhead;
        switch (m -> op)
        {
            case M_MOVE:
                sx          = clamp (m -> a);
                sy          = clamp (m -> b);
                mouse.cx    = wrap (mouse.cx + sx);
                mouse.cy    = wrap (mouse.cy + sy);
                mouse.stepsx  = mouse.stepsx + (uint32_t) sx;
                mouse.stepsy  = mouse.stepsy + (uint32_t) sy;
                m -> a      = m -> a - sx;
                m -> b      = m -> b - sy;
                if ((m -> a == 0) && (m -> b == 0))  m -> op = M_WAIT, m -> a = 0;
                break;
            case M_DOWN:  mouse.buttons  = (uint8_t) (mouse.buttons |  m -> a);  break;
            case M_UP:    mouse.buttons  = (uint8_t) (mouse.buttons & ~m -> a);  break;
            case M_WAIT:  m -> a  = m -> a - 1;  break;
        }
        mouse.frames  = mouse.frames + 1;
        if ((m -> op == M_WAIT) && (m -> a <= 0))
        {
            // finished (a move that ran out, a button change, or the last waiting frame)
            mouse.qhead   = (mouse.qhead + 1) % MQ_MAX;
            mouse.qcount  = mouse.qcount - 1;
        }
        else if (m -> op != M_WAIT && m -> op != M_MOVE)
        {
            mouse.qhead   = (mouse.qhead + 1) % MQ_MAX;
            mouse.qcount  = mouse.qcount - 1;
        }
    }
    present ();
}

////////////////////////////////////////////////////////////////////////////////////////
//
// the device
//
int  v32mouse_attach (int port)
{
    if ((port < 0) || (port >= V32_NUM_GAMEPADS))
    {
        fprintf (stderr, "[ERROR] mouse: no gamepad %d (valid: 0-3)\n", port);
        return (-1);
    }
    if (port == v32io_kbd_port ())
    {
        fprintf (stderr, "[ERROR] mouse: gamepad %d has the keyboard on it\n", port);
        return (-1);
    }
    if ((mouse.port >= 0) && (mouse.port != port))
    {
        v32mouse_detach ();
    }
    mouse.port     = port;
    gamepad_connect ((uint8_t) port, TRUE);
    mouse.cx       = 1;                                 // at rest after power on
    mouse.cy       = 1;
    mouse.buttons  = 0;
    mouse.qhead    = 0;
    mouse.qcount   = 0;
    present ();
    return (0);
}

void  v32mouse_detach (void)
{
    if (mouse.port >= 0)
    {
        gamepad_connect ((uint8_t) mouse.port, FALSE);
    }
    mouse.port    = -1;
    mouse.qhead   = 0;
    mouse.qcount  = 0;
}

int  v32mouse_port (void)
{
    return (mouse.port);
}

void  v32mouse_show (void)
{
    if (mouse.port < 0)
    {
        fprintf (stdout, "v32mouse: not attached\n");
        return;
    }
    fprintf (stdout, "v32mouse: on gamepad %d (%s)\n", mouse.port,
                     (gamepad[mouse.port].connected == TRUE) ? "connected" : "disconnected");
    fprintf (stdout, "  counters: x %d   y %d   buttons: %s%s%s%s\n", mouse.cx, mouse.cy,
                     (mouse.buttons & B_LEFT)   ? "left "   : "", (mouse.buttons & B_RIGHT) ? "right " : "",
                     (mouse.buttons & B_MIDDLE) ? "middle " : "", (mouse.buttons == 0)      ? "none"    : "");
    fprintf (stdout, "  actions waiting: %d of %d   delivered: %u frames   steps: x %d y %d   dropped: %u\n",
                     mouse.qcount, MQ_MAX, mouse.frames, (int32_t) mouse.stepsx, (int32_t) mouse.stepsy, mouse.dropped);
}
