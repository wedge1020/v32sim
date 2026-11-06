#include "defines.h"
#include "memc.h"
#include "devhost.h"
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <ctype.h>

////////////////////////////////////////////////////////////////////////////////////////
//
// devhost.c: a native v32dev host for the simulator (storage and serial devices)
//
// A port of tools/v32dev.py (v32io project; PROTOCOL.md section 2): Channel, StorageDevice
// and SerialDevice, word for word. The differences are only where the host lives: the
// words it reads and writes are the simulator's card memory, not a file; a device is
// polled at every frame boundary (and, with --v32dev-doorbell, at once when the program
// rings); the heartbeat moves once per frame; and the session number is a counter,
// so that a run is repeatable. Nothing here uses wall-clock time.
//
////////////////////////////////////////////////////////////////////////////////////////

#define  MSG_MAX         2020
#define  RING            DEV_RING_WORDS

#define  HOST_ATTACHED   1
#define  ST_READY        1
#define  ST_HUP          4

#define  TYPE_STORAGE    ((1u << 24) | 1)
#define  TYPE_SERIAL     ((2u << 24) | 1)

#define  OP_READ         1
#define  OP_WRITE        2
#define  OP_FLUSH        3
#define  OP_REPLY        128
#define  S_OK            0
#define  S_EINVAL        1
#define  S_ERANGE        2
#define  S_EROFS         3
#define  S_EIO           4
#define  FL_RO           1
#define  FL_BYTES        2

typedef struct
{
    uint32_t   op;
    uint32_t   n;
    uint32_t  *w;
} msg_t;

typedef struct
{
    int        kind;                    // 0 none, 1 storage, 2 serial
    char       spec[1024];
    int        running;
    // the channel, seen from the host
    uint32_t   cart_session;
    uint32_t  *pend;
    uint32_t   npend, cpend;
    uint32_t   bad;
    uint8_t    warned;
    // storage
    int        fd;
    char       path[768];
    int        bytes_mode, readonly;
    uint32_t   sector_words, word_size, sector_bytes, max_sectors;
    uint64_t   sectors;
    int64_t    create;
    msg_t     *out;
    uint32_t   nout, cout;
    uint32_t   st_read, st_write, st_flush, st_bad;
    // serial
    int        echo, hangup_opt, hung_up, hup_req, use_stdio;
    int64_t    hup_after;
    uint64_t   received, sent;
    uint32_t  *inbox;
    uint32_t   nin, cin;
    char       infile[768], outfile[768];
    FILE      *sink;
} dh_t;

static dh_t      dh[DEV_CHANNELS];
static uint8_t   stdin_ok     = FALSE;
static uint8_t   stdin_eof    = FALSE;
static uint8_t   failed       = FALSE;
static uint32_t  sessions     = 0;
static uint8_t   exit_hooked  = FALSE;
uint8_t          devhost_force = FALSE;

////////////////////////////////////////////////////////////////////////////////////////
//
// small helpers
//
static void  push (uint32_t **buf, uint32_t *n, uint32_t *cap, const uint32_t *w, uint32_t count)
{
    if (*n + count > *cap)
    {
        uint32_t  want  = (*n + count) * 2 + 64;
        uint32_t *nb    = (uint32_t *) realloc (*buf, (size_t) want * 4);

        if (nb == NULL)
        {
            fprintf (stderr, "[ERROR:DEVHOST] out of memory\n");
            exit (1);
        }
        *buf  = nb;
        *cap  = want;
    }
    if (count > 0)
    {
        memcpy (*buf + *n, w, (size_t) count * 4);
        *n  = *n + count;
    }
}

static void  drop (uint32_t *buf, uint32_t *n, uint32_t count)
{
    if (count >= *n)
    {
        *n  = 0;
        return;
    }
    memmove (buf, buf + count, (size_t) (*n - count) * 4);
    *n  = *n - count;
}

static uint32_t  hp_of (int ch)  { return (DEV_HOST_START + (uint32_t) ch * DEV_PART_WORDS); }
static uint32_t  cp_of (int ch)  { return (DEV_CART_START + (uint32_t) ch * DEV_PART_WORDS); }

////////////////////////////////////////////////////////////////////////////////////////
//
// the channel (v32dev.py: class Channel)
//
static void  ch_reset (dh_t *d, int ch)
{
    memc_word_set (hp_of (ch) + DEV_HP_H2C_HEAD, memc_word (cp_of (ch) + DEV_CP_H2C_TAIL) % RING);
    memc_word_set (hp_of (ch) + DEV_HP_C2H_TAIL, memc_word (cp_of (ch) + DEV_CP_C2H_HEAD) % RING);
    d -> npend  = 0;
}

static void  ch_beat (int ch)
{
    memc_word_set (hp_of (ch) + DEV_HP_BEAT, memc_word (hp_of (ch) + DEV_HP_BEAT) + 1);
}

static void  ch_status (int ch, uint32_t st)
{
    memc_word_set (hp_of (ch) + DEV_HP_STATUS, st);
}

static void  ch_announce (dh_t *d, int ch, uint32_t type, const uint32_t *desc, int ndesc, uint32_t status)
{
    int       k  = 0;
    uint32_t  hp = hp_of (ch);

    memc_word_set (hp + DEV_HP_FLAGS, 0);
    ch_reset (d, ch);
    sessions  = sessions + 1;
    memc_word_set (hp + DEV_HP_SESSION, 0x32000000u + sessions);
    for (k = 0; k < 8; k = k + 1)
    {
        memc_word_set (hp + DEV_HP_DESC + (uint32_t) k, (k < ndesc) ? desc[k] : 0);
    }
    ch_status (ch, status);
    memc_word_set (hp + DEV_HP_TYPE, type);
    ch_beat (ch);
    memc_word_set (hp + DEV_HP_FLAGS, HOST_ATTACHED);
}

static void  ch_withdraw (int ch)
{
    memc_word_set (hp_of (ch) + DEV_HP_FLAGS,  0);
    memc_word_set (hp_of (ch) + DEV_HP_STATUS, 0);
}

static int  ch_check_cart (dh_t *d, int ch)
{
    uint32_t  session  = memc_word (cp_of (ch) + DEV_CP_SESSION);

    if (session != d -> cart_session)
    {
        d -> cart_session  = session;
        d -> npend         = 0;
        return (1);
    }
    return (0);
}

static int  ch_corrupt (dh_t *d, int ch)
{
    if (d -> warned == FALSE)
    {
        d -> warned  = TRUE;
        fprintf (stderr, "[WARNING:DEVHOST] channel %d has corrupt ring indices\n", ch);
    }
    return (0);
}

// words the host can still put in the host to cart ring; -1 if the indices are corrupt
static int32_t  ch_free (dh_t *d, int ch)
{
    uint32_t  head  = memc_word (hp_of (ch) + DEV_HP_H2C_HEAD);
    uint32_t  tail  = memc_word (cp_of (ch) + DEV_CP_H2C_TAIL);

    if ((head >= RING) || (tail >= RING))
    {
        ch_corrupt (d, ch);
        return (-1);
    }
    return ((int32_t) ((tail + RING - head - 1) % RING));
}

// all or nothing: data first, then the head index
static int  ch_write (dh_t *d, int ch, const uint32_t *w, uint32_t n)
{
    int32_t   room  = ch_free (d, ch);
    uint32_t  head  = 0;
    uint32_t  base  = hp_of (ch) + DEV_CONTROL_WORDS;
    uint32_t  first = 0;
    uint32_t  k     = 0;

    if ((room < 0) || (n > (uint32_t) room))
    {
        return (0);
    }
    if (n == 0)
    {
        return (1);
    }
    head   = memc_word (hp_of (ch) + DEV_HP_H2C_HEAD);
    first  = (n < RING - head) ? n : (RING - head);
    for (k = 0; k < first; k = k + 1)
    {
        memc_word_set (base + head + k, w[k]);
    }
    for (k = first; k < n; k = k + 1)
    {
        memc_word_set (base + (k - first), w[k]);
    }
    memc_word_set (hp_of (ch) + DEV_HP_H2C_HEAD, (head + n) % RING);
    return (1);
}

static int  ch_send (dh_t *d, int ch, uint32_t op, const uint32_t *payload, uint32_t n)
{
    uint32_t  *w  = (uint32_t *) malloc (((size_t) n + 2) * 4);
    int        r  = 0;

    if (w == NULL)
    {
        return (0);
    }
    w[0]  = n;
    w[1]  = op;
    if (n > 0)
    {
        memcpy (w + 2, payload, (size_t) n * 4);
    }
    r  = ch_write (d, ch, w, n + 2);
    free (w);
    return (r);
}

// takes every word available in the cart to host ring; returns how many were appended to buf
static uint32_t  ch_read (dh_t *d, int ch, uint32_t **buf, uint32_t *n, uint32_t *cap)
{
    uint32_t  head   = memc_word (cp_of (ch) + DEV_CP_C2H_HEAD);
    uint32_t  tail   = memc_word (hp_of (ch) + DEV_HP_C2H_TAIL);
    uint32_t  base   = cp_of (ch) + DEV_CONTROL_WORDS;
    uint32_t  count  = 0;
    uint32_t  k      = 0;
    uint32_t  w      = 0;

    if ((head >= RING) || (tail >= RING))
    {
        ch_corrupt (d, ch);
        return (0);
    }
    count  = (head + RING - tail) % RING;
    for (k = 0; k < count; k = k + 1)
    {
        w  = memc_word (base + (tail + k) % RING);
        push (buf, n, cap, &w, 1);
    }
    if (count > 0)
    {
        memc_word_set (hp_of (ch) + DEV_HP_C2H_TAIL, head);
    }
    return (count);
}

////////////////////////////////////////////////////////////////////////////////////////
//
// storage (v32dev.py: class StorageDevice)
//
static void  out_add (dh_t *d, uint32_t op, const uint32_t *w, uint32_t n)
{
    if (d -> nout >= d -> cout)
    {
        d -> cout  = d -> cout * 2 + 8;
        d -> out   = (msg_t *) realloc (d -> out, (size_t) d -> cout * sizeof (msg_t));
        if (d -> out == NULL)
        {
            fprintf (stderr, "[ERROR:DEVHOST] out of memory\n");
            exit (1);
        }
    }
    d -> out[d -> nout].op  = op;
    d -> out[d -> nout].n   = n;
    d -> out[d -> nout].w   = (uint32_t *) malloc (((size_t) n + 1) * 4);
    if (n > 0)
    {
        memcpy (d -> out[d -> nout].w, w, (size_t) n * 4);
    }
    d -> nout  = d -> nout + 1;
}

static void  out_clear (dh_t *d)
{
    uint32_t  k  = 0;

    for (k = 0; k < d -> nout; k = k + 1)
    {
        free (d -> out[k].w);
    }
    d -> nout  = 0;
}

static void  reply2 (dh_t *d, uint32_t op, uint32_t tag, uint32_t status)
{
    uint32_t  w[2];

    w[0]  = tag;
    w[1]  = status;
    out_add (d, OP_REPLY + op, w, 2);
}

static void  st_handle (dh_t *d, uint32_t op, const uint32_t *p, uint32_t n)
{
    uint32_t  tag     = 0;
    uint64_t  sector  = 0;
    uint64_t  count   = 0;
    uint32_t  bytes   = 0;
    uint32_t  k       = 0;
    uint32_t *rep     = NULL;
    uint8_t  *raw     = NULL;
    ssize_t   r       = 0;

    if (n == 0)
    {
        reply2 (d, op, 0, S_EINVAL);
        return;
    }
    tag  = p[0];
    if (op == OP_FLUSH)
    {
        d -> st_flush  = d -> st_flush + 1;
        reply2 (d, op, tag, (0 == fsync (d -> fd)) ? S_OK : S_EIO);
        return;
    }
    if (((op != OP_READ) && (op != OP_WRITE)) || (n < 3))
    {
        d -> st_bad  = d -> st_bad + 1;
        reply2 (d, op, tag, S_EINVAL);
        return;
    }
    sector  = p[1];
    count   = p[2];
    if ((count < 1) || (count > d -> max_sectors))
    {
        reply2 (d, op, tag, S_EINVAL);
        return;
    }
    if (sector + count > d -> sectors)
    {
        reply2 (d, op, tag, S_ERANGE);
        return;
    }
    bytes  = (uint32_t) count * d -> sector_bytes;
    raw    = (uint8_t *) malloc ((size_t) bytes + 4);
    if (raw == NULL)
    {
        reply2 (d, op, tag, S_EIO);
        return;
    }
    if (op == OP_READ)
    {
        if (n != 3)
        {
            reply2 (d, op, tag, S_EINVAL);
        }
        else
        {
            r  = pread (d -> fd, raw, bytes, (off_t) (sector * d -> sector_bytes));
            if (r != (ssize_t) bytes)
            {
                reply2 (d, op, tag, S_EIO);
            }
            else
            {
                uint32_t  words  = bytes / d -> word_size;

                rep  = (uint32_t *) malloc (((size_t) words + 2) * 4);
                rep[0]  = tag;
                rep[1]  = S_OK;
                for (k = 0; k < words; k = k + 1)
                {
                    rep[2 + k]  = (d -> bytes_mode == TRUE) ? raw[k] :
                                  ((uint32_t) raw[4*k] | ((uint32_t) raw[4*k+1] << 8) |
                                   ((uint32_t) raw[4*k+2] << 16) | ((uint32_t) raw[4*k+3] << 24));
                }
                d -> st_read  = d -> st_read + (uint32_t) count;
                out_add (d, OP_REPLY + op, rep, words + 2);
                free (rep);
            }
        }
    }
    else if (d -> readonly == TRUE)
    {
        reply2 (d, op, tag, S_EROFS);
    }
    else if (n != 3 + count * d -> sector_words)
    {
        reply2 (d, op, tag, S_EINVAL);
    }
    else
    {
        uint32_t  words  = (uint32_t) count * d -> sector_words;

        for (k = 0; k < words; k = k + 1)
        {
            if (d -> bytes_mode == TRUE)
            {
                raw[k]  = (uint8_t) (p[3 + k] & 0xFF);
            }
            else
            {
                raw[4*k]    = (uint8_t) (p[3 + k]);
                raw[4*k+1]  = (uint8_t) (p[3 + k] >> 8);
                raw[4*k+2]  = (uint8_t) (p[3 + k] >> 16);
                raw[4*k+3]  = (uint8_t) (p[3 + k] >> 24);
            }
        }
        r  = pwrite (d -> fd, raw, bytes, (off_t) (sector * d -> sector_bytes));
        if (r != (ssize_t) bytes)
        {
            reply2 (d, op, tag, S_EIO);
        }
        else
        {
            d -> st_write  = d -> st_write + (uint32_t) count;
            reply2 (d, op, tag, S_OK);
        }
    }
    free (raw);
}

static int  out_flush (dh_t *d, int ch)
{
    int  did  = 0;

    while (d -> nout > 0)
    {
        if (0 == ch_send (d, ch, d -> out[0].op, d -> out[0].w, d -> out[0].n))
        {
            break;
        }
        free (d -> out[0].w);
        memmove (d -> out, d -> out + 1, (size_t) (d -> nout - 1) * sizeof (msg_t));
        d -> nout  = d -> nout - 1;
        did        = 1;
    }
    return (did);
}

// complete messages in pend -> handler
static void  ch_messages (dh_t *d, int ch, void (*handle) (dh_t *, uint32_t, const uint32_t *, uint32_t))
{
    uint32_t  words  = 0;

    ch_read (d, ch, &d -> pend, &d -> npend, &d -> cpend);
    while (d -> npend >= 2)
    {
        words  = d -> pend[0];
        if (words > MSG_MAX)
        {
            d -> bad  = d -> bad + 1;
            drop (d -> pend, &d -> npend, 1);
            continue;
        }
        if (d -> npend < words + 2)
        {
            break;
        }
        handle (d, d -> pend[1], d -> pend + 2, words);
        drop (d -> pend, &d -> npend, words + 2);
    }
}

static int  st_poll (dh_t *d, int ch)
{
    int  did  = 0;

    if (ch_check_cart (d, ch) != 0)
    {
        out_clear (d);
        did  = 1;
    }
    if (d -> nout > 0)
    {
        if (out_flush (d, ch) != 0)  did = 1;
        if (d -> nout > 0)           return (did);
    }
    {
        uint32_t  before  = d -> nout;

        ch_messages (d, ch, st_handle);
        if (d -> nout != before)  did = 1;
    }
    if (out_flush (d, ch) != 0)  did = 1;
    return (did);
}

////////////////////////////////////////////////////////////////////////////////////////
//
// serial (v32dev.py: class SerialDevice)
//
static void  sink_words (dh_t *d, const uint32_t *w, uint32_t n)
{
    uint32_t  k  = 0;

    if (d -> sink != NULL)
    {
        for (k = 0; k < n; k = k + 1)
        {
            fputc ((int) (w[k] & 0xFF), d -> sink);
        }
        fflush (d -> sink);
    }
}

static void  load_infile (dh_t *d)
{
    FILE     *f  = NULL;
    int       c  = 0;
    uint32_t  w  = 0;

    if (d -> infile[0] == 0)
    {
        return;
    }
    f  = fopen (d -> infile, "rb");
    if (f == NULL)
    {
        fprintf (stderr, "[WARNING:DEVHOST] could not read '%s'\n", d -> infile);
        return;
    }
    for (;;)
    {
        c  = fgetc (f);
        if (c == EOF)  break;
        w  = (uint32_t) c;
        push (&d -> inbox, &d -> nin, &d -> cin, &w, 1);
    }
    fclose (f);
}

static void  read_stdin (dh_t *d)
{
    struct pollfd  p;
    uint8_t        buf[4096];
    ssize_t        n  = 0;
    ssize_t        k  = 0;
    uint32_t       w  = 0;

    if ((d -> use_stdio == FALSE) || (stdin_ok == FALSE) || (stdin_eof == TRUE))
    {
        return;
    }
    p.fd      = 0;
    p.events  = POLLIN;
    p.revents = 0;
    if (poll (&p, 1, 0) <= 0)
    {
        return;
    }
    n  = read (0, buf, sizeof (buf));
    if (n <= 0)
    {
        stdin_eof    = TRUE;
        d -> hup_req = TRUE;                 // end of input
        return;
    }
    for (k = 0; k < n; k = k + 1)
    {
        w  = buf[k];
        push (&d -> inbox, &d -> nin, &d -> cin, &w, 1);
    }
}

static int  se_poll (dh_t *d, int ch)
{
    int       did    = 0;
    uint32_t  n0     = 0;
    uint32_t *got    = NULL;
    uint32_t  ngot   = 0, cgot = 0;
    uint32_t  got_n  = 0;
    int32_t   room   = 0;
    uint32_t  chunk  = 0;

    if (ch_check_cart (d, ch) != 0)
    {
        d -> nin       = 0;
        d -> received  = 0;
        load_infile (d);
        if (d -> hung_up == TRUE)             // the program came back: a new conversation
        {
            d -> hung_up  = FALSE;
            ch_status (ch, ST_READY);
        }
        did  = 1;
    }
    read_stdin (d);

    n0     = 0;
    got_n  = ch_read (d, ch, &got, &ngot, &cgot);
    (void) n0;
    if (got_n > 0)
    {
        did  = 1;
        d -> received  = d -> received + got_n;
        sink_words (d, got, got_n);
        if (d -> echo == TRUE)
        {
            push (&d -> inbox, &d -> nin, &d -> cin, got, got_n);
        }
    }
    free (got);

    if (d -> nin > 0)
    {
        room   = ch_free (d, ch);
        chunk  = (room < 0) ? 0 : ((d -> nin < (uint32_t) room) ? d -> nin : (uint32_t) room);
        if ((chunk > 0) && (0 != ch_write (d, ch, d -> inbox, chunk)))
        {
            drop (d -> inbox, &d -> nin, chunk);
            d -> sent  = d -> sent + chunk;
            did        = 1;
        }
    }
    if ((d -> hung_up == FALSE) && (d -> nin == 0) &&
        ((d -> hup_req == TRUE) ||
         ((d -> hangup_opt == TRUE) && (d -> infile[0] != 0)) ||
         ((d -> hup_after >= 0) && (d -> received >= (uint64_t) d -> hup_after))))
    {
        d -> hung_up  = TRUE;
        ch_status (ch, ST_READY | ST_HUP);
        did           = 1;
    }
    return (did);
}

////////////////////////////////////////////////////////////////////////////////////////
//
// specs
//
static int  parse_u (const char *s, int64_t *out)
{
    char *end  = NULL;

    *out  = (int64_t) strtoll (s, &end, 0);
    return ((end != s) && (*end == 0));
}

int  devhost_add (const char *arg)
{
    int         ch    = -1;
    const char *p     = arg;
    const char *eq    = NULL;
    char        spec[1024];
    char       *kind  = NULL;
    char       *rest  = NULL;
    char       *tok   = NULL;
    char       *save  = NULL;
    dh_t       *d     = NULL;
    int         first = 1;
    int64_t     v     = 0;

    if ((p == NULL) || (p[0] < '0') || (p[0] > '3') || (p[1] != '='))
    {
        fprintf (stderr, "[ERROR:DEVHOST] --v32dev-host wants N=SPEC with N from 0 to 3 (got '%s')\n", (arg == NULL) ? "" : arg);
        return (-1);
    }
    ch   = p[0] - '0';
    eq   = p + 2;
    d    = dh + ch;
    if (d -> kind != 0)
    {
        fprintf (stderr, "[ERROR:DEVHOST] channel %d is given twice\n", ch);
        return (-1);
    }
    snprintf (spec, sizeof (spec), "%s", eq);
    kind  = spec;
    rest  = strchr (spec, ':');
    if (rest != NULL)
    {
        *rest  = 0;
        rest   = rest + 1;
    }
    for (tok = kind; *tok; tok = tok + 1)  *tok = (char) tolower ((unsigned char) *tok);

    d -> fd         = -1;
    d -> hup_after  = -1;
    d -> create     = -1;
    snprintf (d -> spec, sizeof (d -> spec), "%s", eq);

    if (0 == strcmp (kind, "storage"))
    {
        if ((rest == NULL) || (rest[0] == 0) || (rest[0] == ','))
        {
            fprintf (stderr, "[ERROR:DEVHOST] channel %d: storage needs a file\n", ch);
            return (-1);
        }
        d -> kind  = 1;
        for (tok = strtok_r (rest, ",", &save); tok != NULL; tok = strtok_r (NULL, ",", &save))
        {
            if (first == 1)
            {
                snprintf (d -> path, sizeof (d -> path), "%s", tok);
                first  = 0;
            }
            else if (0 == strcmp (tok, "bytes"))          d -> bytes_mode = TRUE;
            else if (0 == strcmp (tok, "ro"))             d -> readonly   = TRUE;
            else if ((0 == strncmp (tok, "sector=", 7)) && parse_u (tok + 7, &v) && (v > 0))  d -> sector_words = (uint32_t) v;
            else if ((0 == strncmp (tok, "create=", 7)) && parse_u (tok + 7, &v) && (v >= 0)) d -> create = v;
            else
            {
                fprintf (stderr, "[ERROR:DEVHOST] channel %d: unknown storage option: %s\n", ch, tok);
                d -> kind  = 0;
                return (-1);
            }
        }
        if (d -> sector_words == 0)  d -> sector_words = (d -> bytes_mode == TRUE) ? 512 : 128;
        d -> word_size    = (d -> bytes_mode == TRUE) ? 1 : 4;
        d -> sector_bytes = d -> sector_words * d -> word_size;
        d -> max_sectors  = (MSG_MAX - 4) / d -> sector_words;
        if (d -> max_sectors < 1)
        {
            fprintf (stderr, "[ERROR:DEVHOST] channel %d: sector size is too big (at most %d words)\n", ch, MSG_MAX - 4);
            d -> kind  = 0;
            return (-1);
        }
    }
    else if (0 == strcmp (kind, "serial"))
    {
        d -> kind  = 2;
        if ((rest == NULL) || (rest[0] == 0))
        {
            fprintf (stderr, "[ERROR:DEVHOST] channel %d: serial needs stdio or echo[,hup=N]\n", ch);
            d -> kind  = 0;
            return (-1);
        }
        for (tok = strtok_r (rest, ",", &save); tok != NULL; tok = strtok_r (NULL, ",", &save))
        {
            if (0 == strcmp (tok, "stdio"))               { d -> use_stdio = TRUE; }
            else if (0 == strcmp (tok, "echo"))           d -> echo = TRUE;
            else if (0 == strcmp (tok, "hangup"))         d -> hangup_opt = TRUE;
            else if ((0 == strncmp (tok, "hup=", 4)) && parse_u (tok + 4, &v) && (v >= 0))   d -> hup_after = v;
            else if (0 == strncmp (tok, "in=", 3))        snprintf (d -> infile,  sizeof (d -> infile),  "%s", tok + 3);
            else if (0 == strncmp (tok, "out=", 4))       snprintf (d -> outfile, sizeof (d -> outfile), "%s", tok + 4);
            else
            {
                fprintf (stderr, "[ERROR:DEVHOST] channel %d: unknown serial option: %s\n", ch, tok);
                d -> kind  = 0;
                return (-1);
            }
        }
        if ((d -> use_stdio == FALSE) && (d -> echo == FALSE) && (d -> infile[0] == 0) && (d -> outfile[0] == 0) && (d -> hup_after < 0))
        {
            fprintf (stderr, "[ERROR:DEVHOST] channel %d: serial needs stdio or echo[,hup=N]\n", ch);
            d -> kind  = 0;
            return (-1);
        }
    }
    else
    {
        fprintf (stderr, "[ERROR:DEVHOST] channel %d: unknown device: %s\n", ch, eq);
        return (-1);
    }
    if (exit_hooked == FALSE)
    {
        exit_hooked  = TRUE;
        atexit (devhost_stop);
    }
    return (0);
}

uint8_t  devhost_configured (void)
{
    int  ch  = 0;

    for (ch = 0; ch < DEV_CHANNELS; ch = ch + 1)
    {
        if (dh[ch].kind != 0)  return (TRUE);
    }
    return (FALSE);
}

void  devhost_fail (void)
{
    failed  = TRUE;
}

uint8_t  devhost_ok (void)
{
    return (failed == FALSE);
}

uint8_t  devhost_serves (int ch)
{
    return ((ch >= 0) && (ch < DEV_CHANNELS) && (dh[ch].kind != 0) && (dh[ch].running != 0));
}

void  devhost_stdin (uint8_t on)
{
    stdin_ok  = on;
}

int  devhost_types (uint32_t *types)
{
    int  ch = 0, n = 0;

    for (ch = 0; ch < DEV_CHANNELS; ch = ch + 1)
    {
        types[ch]  = (dh[ch].kind == 1) ? TYPE_STORAGE : (dh[ch].kind == 2) ? TYPE_SERIAL : 0;
        if (dh[ch].kind != 0)  n = n + 1;
    }
    return (n);
}

////////////////////////////////////////////////////////////////////////////////////////
//
// start / stop
//
static void  dev_open (dh_t *d, int ch)
{
    struct stat  st;
    uint32_t     desc[4];
    uint32_t     flags  = 0;

    d -> running  = 0;
    d -> cart_session  = memc_word (cp_of (ch) + DEV_CP_SESSION);
    d -> npend = d -> nin = d -> nout = 0;
    d -> received = d -> sent = 0;
    d -> hung_up = d -> hup_req = FALSE;

    if (d -> kind == 1)
    {
        if ((d -> create >= 0) && (0 != stat (d -> path, &st)))
        {
            int  f  = open (d -> path, O_WRONLY | O_CREAT, 0666);

            if ((f < 0) || (0 != ftruncate (f, (off_t) (d -> create * d -> sector_bytes))))
            {
                fprintf (stderr, "[ERROR:DEVHOST] channel %d: cannot create '%s'\n", ch, d -> path);
                failed  = TRUE;
                if (f >= 0)  close (f);
                return;
            }
            close (f);
        }
        d -> fd  = open (d -> path, (d -> readonly == TRUE) ? O_RDONLY : O_RDWR);
        if ((d -> fd < 0) || (0 != fstat (d -> fd, &st)))
        {
            fprintf (stderr, "[ERROR:DEVHOST] channel %d: cannot open '%s'\n", ch, d -> path);
            failed  = TRUE;
            return;
        }
        d -> sectors  = (uint64_t) st.st_size / d -> sector_bytes;
        flags         = ((d -> readonly == TRUE) ? FL_RO : 0) | ((d -> bytes_mode == TRUE) ? FL_BYTES : 0);
        desc[0]  = d -> sector_words;
        desc[1]  = (uint32_t) d -> sectors;
        desc[2]  = flags;
        desc[3]  = d -> max_sectors;
        ch_announce (d, ch, TYPE_STORAGE, desc, 4, ST_READY);
    }
    else
    {
        d -> sink  = NULL;
        if (d -> use_stdio == TRUE)
        {
            d -> sink  = stdout;
        }
        if (d -> outfile[0] != 0)
        {
            d -> sink  = fopen (d -> outfile, "wb");
            if (d -> sink == NULL)
            {
                fprintf (stderr, "[ERROR:DEVHOST] channel %d: cannot write '%s'\n", ch, d -> outfile);
                failed  = TRUE;
                return;
            }
        }
        load_infile (d);
        ch_announce (d, ch, TYPE_SERIAL, NULL, 0, ST_READY);
    }
    d -> running  = 1;
}

void  devhost_start (void)
{
    int  ch  = 0;

    failed  = FALSE;
    for (ch = 0; ch < DEV_CHANNELS; ch = ch + 1)
    {
        if (dh[ch].kind != 0)
        {
            dev_open (dh + ch, ch);
            if (dh[ch].running != 0)
            {
                fprintf (verbose, "[DEVHOST] channel %d: %s\n", ch, dh[ch].spec);
            }
        }
    }
}

void  devhost_stop (void)
{
    int  ch  = 0;

    for (ch = 0; ch < DEV_CHANNELS; ch = ch + 1)
    {
        dh_t  *d  = dh + ch;

        if (d -> running != 0)
        {
            ch_withdraw (ch);
        }
        d -> running  = 0;
        if (d -> fd >= 0)
        {
            close (d -> fd);
            d -> fd  = -1;
        }
        if ((d -> sink != NULL) && (d -> sink != stdout))
        {
            fclose (d -> sink);
        }
        d -> sink  = NULL;
        out_clear (d);
    }
}

void  devhost_resync (void)
{
    int  ch  = 0;

    for (ch = 0; ch < DEV_CHANNELS; ch = ch + 1)
    {
        if (dh[ch].running != 0)
        {
            dh[ch].cart_session  = memc_word (cp_of (ch) + DEV_CP_SESSION);
            dh[ch].npend         = 0;
            out_clear (dh + ch);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////
//
// work
//
static int  poll_one (int ch)
{
    dh_t  *d  = dh + ch;

    if (d -> running == 0)
    {
        return (0);
    }
    return ((d -> kind == 1) ? st_poll (d, ch) : se_poll (d, ch));
}

void  devhost_poll (int ch)
{
    int  rounds  = 0;

    while ((rounds < 1000) && (0 != poll_one (ch)))
    {
        rounds  = rounds + 1;
    }
}

void  devhost_frame (void)
{
    int  ch = 0;

    for (ch = 0; ch < DEV_CHANNELS; ch = ch + 1)
    {
        if (dh[ch].running != 0)  ch_beat (ch);
    }
    for (ch = 0; ch < DEV_CHANNELS; ch = ch + 1)
    {
        devhost_poll (ch);
    }
}

void  devhost_describe (int ch, char *out, size_t len)
{
    dh_t  *d  = dh + ch;

    out[0]  = 0;
    if ((ch < 0) || (ch >= DEV_CHANNELS) || (d -> kind == 0))
    {
        return;
    }
    if (d -> kind == 1)
    {
        snprintf (out, len, "native host: storage '%s'%s%s, sector %u words, %llu sectors, read %u write %u flush %u bad %u%s",
                  d -> path, (d -> readonly == TRUE) ? " ro" : "", (d -> bytes_mode == TRUE) ? " bytes" : "",
                  d -> sector_words, (unsigned long long) d -> sectors, d -> st_read, d -> st_write, d -> st_flush, d -> st_bad,
                  (d -> running != 0) ? "" : " (not running)");
    }
    else
    {
        snprintf (out, len, "native host: serial '%s', %llu words from the program, %llu to it%s%s%s",
                  d -> spec, (unsigned long long) d -> received, (unsigned long long) d -> sent,
                  (d -> hung_up == TRUE) ? ", hung up" : "", (d -> bad != 0) ? ", corrupt stream seen" : "",
                  (d -> running != 0) ? "" : " (not running)");
    }
}
