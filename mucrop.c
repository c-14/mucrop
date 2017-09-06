#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>

#include <MagickWand/MagickWand.h>

#include "util/error.h"

struct mucrop_core {
	MagickWand *wand;

	struct mu_error *errlist;
};

#define RaiseWandException(wand, errlist) \
{ \
	char *description; \
	ExceptionType severity; \
\
	description = MagickGetException(wand, &severity); \
	MU_PUSH_ERRSTR(errlist, description); \
	description = (char *) MagickRelinquishMemory(description); \
}

int read_image(struct mucrop_core *core, const char *filename)
{
	MagickBooleanType status;

	status = MagickReadImage(core->wand, filename);
	if (status == MagickFalse) {
		RaiseWandException(core->wand, &core->errlist);
		return -1;
	}

	// Get Output Resolution/Preferred Output Format
	// Convert Copy of Image to Format (RGBA/YUV/Whatever)
	// GetImageBlob and destroy copy of Image
	/* MagickResetIterator(core->wand); */
	/* MagickSetImageFormat(core->wand,"xpm"); */
	/* image = MagickGetImageBlob(core->wand,&length); */


	return 0;
}

int main(int argc, const char *argv[])
{
	struct mucrop_core core;
	int ret = 0;

	MagickWandGenesis();
	core.wand = NewMagickWand();

	core.errlist = create_errlist(3);
	if (core.errlist == NULL) {
		perror("malloc");
		ret = EX_OSERR;
		goto fail;
	}

	/* for (size_t i = 1; i < argc; i++) { */
	/* 	read_image(wand, argv[i]); */
	/* } */


fail:
	free_errlist(&core.errlist);

	core.wand = DestroyMagickWand(core.wand);
	MagickWandTerminus();

	return ret;
}
