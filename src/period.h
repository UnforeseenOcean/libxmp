#ifndef LIBXMP_PERIOD_H
#define LIBXMP_PERIOD_H

/* Macros for period conversion */
#define NOTE_B0		11
#define NOTE_Bb0	(NOTE_B0 + 1)
#define MAX_NOTE	(NOTE_B0 * 8)
#define MAX_PERIOD	0x1c56
#define MIN_PERIOD_A	0x0071
#define MAX_PERIOD_A	0x0358
#define MIN_PERIOD_L	0x0000
#define MAX_PERIOD_L	0x1e00
#define MIN_NOTE_MOD	48
#define MAX_NOTE_MOD	83

double	note_to_period		(struct context_data *, int, int, int, double);
int	note_to_period_mix	(int, int);
int	period_to_note		(int);
int	period_to_bend		(struct context_data *, double, int, int, double);
void	c2spd_to_note		(int, int *, int *);

#endif /* LIBXMP_PERIOD_H */
