/*
 *  util.c
 *
 *  Copyright (C) 2020 Michael Straube <michael.straubej@gmail.com>
 *  Copyright (C) 2006-2020 Pacman Development Team <pacman-dev@archlinux.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "util.h"

/** Wrapper around fgets() which properly handles EINTR
 * @param s string to read into
 * @param size maximum length to read
 * @param stream stream to read from
 * @return value returned by fgets()
 */
char *safe_fgets(char *s, int size, FILE *stream)
{
	char *ret;
	int errno_save = errno, ferror_save = ferror(stream);

	while (!(ret = fgets(s, size, stream)) && !feof(stream)) {
		if (errno == EINTR) {
			/* clear any errors we set and try again */
			errno = errno_save;
			if (!ferror_save) {
				clearerr(stream);
			}
		} else {
			break;
		}
	}
	return ret;
}

/* Trim whitespace and newlines from a string
 */
size_t strtrim(char *str)
{
	char *end, *pch = str;

	if (!str || *str == '\0') {
		/* string is empty, so we're done. */
		return 0;
	}

	while (isspace((unsigned char)*pch)) {
		pch++;
	}
	if (pch != str) {
		size_t len = strlen(pch);
		/* check if there wasn't anything but whitespace in the string. */
		if (len == 0) {
			*str = '\0';
			return 0;
		}
		memmove(str, pch, len + 1);
		pch = str;
	}

	end = (str + strlen(str) - 1);
	while (isspace((unsigned char)*end)) {
		end--;
	}
	*++end = '\0';

	return end - pch;
}

void wordsplit_free(char **ws)
{
	if (ws) {
		char **c;
		for (c = ws; *c; c++) {
			free(*c);
		}
		free(ws);
	}
}

char **wordsplit(const char *str)
{
	const char *c = str, *end;
	char **out = NULL, **outsave;
	size_t count = 0;

	if (!str) {
		errno = EINVAL;
		return NULL;
	}

	for (c = str; isspace(*c); c++);
	while (*c) {
		size_t wordlen = 0;

		/* extend our array */
		outsave = out;
		out = realloc(out, (count + 1) * sizeof(char*));
		if (!(out = realloc(out, (count + 1) * sizeof(char*)))) {
			out = outsave;
			goto error;
		}

		/* calculate word length and check for unbalanced quotes */
		for (end = c; *end && !isspace(*end); end++) {
			if (*end == '\'' || *end == '"') {
				char quote = *end;
				while (*(++end) && *end != quote) {
					if (*end == '\\' && *(end + 1) == quote) {
						end++;
					}
					wordlen++;
				}
				if (*end != quote) {
					errno = EINVAL;
					goto error;
				}
			} else {
				if (*end == '\\' && (end[1] == '\'' || end[1] == '"')) {
					end++; /* skip the '\\' */
				}
				wordlen++;
			}
		}

		if (wordlen == (size_t)(end - c)) {
			/* no internal quotes or escapes, copy it the easy way */
			if (!(out[count++] = strndup(c, wordlen))) {
				goto error;
			}
		} else {
			/* manually copy to remove quotes and escapes */
			char *dest = out[count++] = malloc(wordlen + 1);
			if (!dest) {
				goto error;
			}
			while (c < end) {
				if (*c == '\'' || *c == '"') {
					char quote = *c;
					/* we know there must be a matching end quote,
					 * no need to check for '\0' */
					for (c++; *c != quote; c++) {
						if (*c == '\\' && *(c + 1) == quote) {
							c++;
						}
						*(dest++) = *c;
					}
					c++;
				} else {
					if (*c == '\\' && (c[1] == '\'' || c[1] == '"')) {
						c++; /* skip the '\\' */
					}
					*(dest++) = *(c++);
				}
			}
			*dest = '\0';
		}

		if (*end == '\0') {
			break;
		} else {
			for (c = end + 1; isspace(*c); c++);
		}
	}

	outsave = out;
	if (!(out = realloc(out, (count + 1) * sizeof(char*)))) {
		out = outsave;
		goto error;
	}

	out[count++] = NULL;

	return out;

error:
	/* can't use wordsplit_free here because NULL has not been appended */
	while (count) {
		free(out[--count]);
	}
	free(out);
	return NULL;
}
