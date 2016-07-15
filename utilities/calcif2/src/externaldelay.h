#ifndef __EXTERNAL_DELAY_H__
#define __EXTERNAL_DELAY_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	double mjd;	/* UTC */
	double delay;	/* seconds */
	double dry;	/* seconds */
	double wet;	/* seconds */
} DelayRow;

typedef struct
{
	int nRow;
	DelayRow *rows;
} ExternalDelay;

ExternalDelay *newExternalDelay(const char *filename);

void deleteExternalDelay(ExternalDelay *ed);

int getExternalDelay(const ExternalDelay *ed, double mjd, double *delay, double *dry, double *wet);

#ifdef __cplusplus
}
#endif

#endif
