/*
 *
 *   Copyright (c) 2022, Bernd Rellermeyer
 *
 *   This source code is released for free distribution under the terms of the
 *   GNU General Public License version 2 or (at your option) any later version.
 *
 *   This module contains functions for generating tags from the output of an
 *   external parser.
 *
 *   2022-11-11  Initial release.
 */

#include "general.h"   /* must always come first */
#include "debug.h"
#include "entry.h"
#include "param.h"
#include "parse.h"
#include "read.h"
#include "routines.h"

/* This private include is needed to override Option.customXfmt. */
#define OPTION_WRITE
#include "options_p.h"

/* This private include is needed for language kinds. */
#include "parse_p.h"

#if defined (HAVE_UNISTD_H)
#include <unistd.h>
#endif
#if defined (WIN32) && !defined (__CYGWIN__)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fcntl.h>
#else
#include <sys/wait.h>
#endif
#include <errno.h>
#include <string.h>
#include <jansson.h>

enum externField {
	F_ENCODEDNAME,
	F_SUMMARY
};

static roleDefinition ExternRefRoles [] = {
	{ true, "ref", "reference" }
};

static roleDefinition ExternOtherRoles [] = {
	{ true, "other", "other symbol" }
};

static char *parserCommand = NULL;

static bool setParserCommand (langType language CTAGS_ATTR_UNUSED,
							  const char *name CTAGS_ATTR_UNUSED,
							  const char *arg)
{
	parserCommand = eStrdup (arg);
	return true;
}

static void freeKindDef (kindDefinition *kdef)
{
	eFree (kdef->name);
	eFree (kdef->description);
	eFree (kdef);
}

static void defineExternKind (char letter, const char *name, const char *role)
{
	Assert (letter != '\0');
	Assert (name != NULL);

	langType language = getNamedLanguage ("Extern", 0);
	kindDefinition *kdef = xCalloc (1, kindDefinition);
	char roleChar = role[0];

	kdef->enabled = true;
	kdef->letter = letter;
	kdef->name = eStrdup (name);
	kdef->description = eStrdup (name);
	kdef->referenceOnly = roleChar != 'd';
	kdef->nRoles = roleChar == 'r' || roleChar == 'o' ? 1 : 0;
	kdef->roles = roleChar == 'r' ? ExternRefRoles
		: roleChar == 'o' ? ExternOtherRoles : NULL;

	defineLanguageKind (language, kdef, freeKindDef);
}

typedef struct sTagFormat {
	char *kind;
	char *prefix;
	char *summaryFmt;
	struct sTagFormat *next;
} tagFormat;

static tagFormat *tagFormatList = NULL;

static void addTagFormat (const char *kind, const char *prefix,
						  const char *summaryFmt)
{
	Assert (kind != NULL);

	if (prefix == NULL && summaryFmt == NULL) return;

	tagFormat *format = tagFormatList;

	if (format == NULL)
	{
		tagFormatList = xCalloc (1, tagFormat);
		format = tagFormatList;
		format->kind = eStrdup (kind);
		format->prefix = NULL;
		format->summaryFmt = NULL;
		format->next = NULL;
	}

	while (format != NULL)
	{
		if (strcmp (format->kind, kind) == 0)
		{
			if (prefix != NULL) format->prefix = eStrdup (prefix);
			if (summaryFmt != NULL) format->summaryFmt = eStrdup (summaryFmt);
			break;
		}

		if (format->next == NULL)
		{
			format->next = xCalloc (1, tagFormat);
			format = format->next;
			format->kind = eStrdup (kind);
			format->prefix = NULL;
			format->summaryFmt = NULL;
			format->next = NULL;
		}
		else
			format = format->next;
	}
}

static tagFormat *getTagFormat (int kindIndex)
{
	langType language = getNamedLanguage ("Extern", 0);
	const char *kindName = getLanguageKindName (language, kindIndex);
	tagFormat *format = tagFormatList;

	while (format != NULL)
	{
		if (strcmp (format->kind, kindName) == 0) break;
		format = format->next;
	}

	return format;
}

static bool defineKinds (langType language CTAGS_ATTR_UNUSED,
						 const char *name CTAGS_ATTR_UNUSED,
						 const char *arg)
{
	char *c = (char *)arg;

	while (c != NULL)
	{
		const char *kind = c;
		char *role = NULL;
		char letter = '\0';
		char *prefix = NULL;
		char *summaryFmt = NULL;

		c = strchr (c, ':');

		if (c != NULL)
		{
			*c++ = '\0';
			letter = *c;
			if (letter != '\0') c = strchr (c, ':');
			if (c != NULL) *c++ = '\0';
		}

		if (c != NULL)
		{
			role = c;
			if (role != NULL) c = strchr (c, ':');
			if (c != NULL) *c++ = '\0';
		}

		defineExternKind (letter, kind, role);

		if (c != NULL)
		{
			prefix = c;
			if (prefix != NULL) c = strchr (c, ':');
			if (c != NULL) *c++ = '\0';
		}

		if (c != NULL)
		{
			summaryFmt = c;
			if (summaryFmt != NULL) c = strchr (c, ',');
			if (c != NULL) *c++ = '\0';
		}

		addTagFormat (kind, prefix, summaryFmt);
	}

	return true;
}

static char *xrefFormat = NULL;

static bool setXrefFormat (langType language CTAGS_ATTR_UNUSED,
						   const char *name CTAGS_ATTR_UNUSED,
						   const char *arg)
{
	xrefFormat = eStrdup (arg);
	return true;
}

static paramDefinition ExternParams [] = {
	{
		.name = "parser",
		.desc = "set the parser command (string)",
		.handleParam = setParserCommand
	},
	{
		.name = "kinds",
		.desc = "define and configure parser-specific kinds (string)",
		.handleParam = defineKinds
	},
	{
		.name = "xformat",
		.desc = "set the Xref output format (string)",
		.handleParam = setXrefFormat
	}
};

static char *percentEncode (char *buffer, char *string,
							size_t len, bool force)
{
	static char hex [] = "0123456789ABCDEF";

	size_t l = string == NULL ? 0 : strlen (string);
	if (len > l) len = l;

	char *p = buffer;
	char *c = string;

	for (int i = 0; i < len; ++c, ++i)
	{
		if (force || *c < 0x21 || *c > 0x7E || *c == '%')
		{
			*p++ = '%';
			*p++ = hex [*c >> 4 & 0x0F];
			*p++ = hex [*c & 0x0F];
		}
		else
			*p++ = *c;
	}

	return p;
}

static const char *renderFieldEncodedName (const tagEntryInfo *const tag,
										   const char *value CTAGS_ATTR_UNUSED,
										   vString *buffer)
{
	char *c = (char *)tag->name;
	tagFormat *format = getTagFormat (tag->kindIndex);

	size_t len = c == NULL ? 0 : 3 * strlen (c) + 1;
	size_t prefixLen = format && format->prefix ? strlen (format->prefix) : 0;
	char *b = xCalloc (3 * len + prefixLen + 1, char);

	/* Prefix the tag name if desired. */
	if (prefixLen > 0) b = strncpy (b, format->prefix, len + prefixLen);
	char *p = b + prefixLen;

	if (*c == '!')
		/* Percent-encode a leading exclamation mark as it conflicts with
		 * pseudo-tags when sorting. */
		p = percentEncode (p, c++, 1, true);
	else
	{
		/* Percent-encode the leading character of an unprefixed tag if it
		 * starts with a prefix string, so that it can be distinguished from a
		 * prefixed tag. */
		tagFormat *format = getTagFormat (tag->kindIndex);

		if (format->prefix == NULL || strlen (format->prefix) == 0)
		{
			langType language = getNamedLanguage ("Extern", 0);
			const char *kindName = getLanguageKindName (language,
														tag->kindIndex);
			format = tagFormatList;

			while (format != NULL)
			{
				if (strcmp (kindName, format->kind) != 0
					&& format->prefix != NULL && strlen (format->prefix) > 0
					&& strncmp (c, format->prefix,
								strlen (format->prefix)) == 0)
				{
					p = percentEncode (p, c++, 1, true);
					break;
				}

				format = format->next;
			}
		}
	}

	/* Percent-encode the tag. */
	p = percentEncode (p, c, b + len - p, false);

	vStringNCatS (buffer, b, p - b);
	eFree (b);

	return vStringValue (buffer);
}

static const char *renderFieldSummary (const tagEntryInfo *const tag,
									   const char *value CTAGS_ATTR_UNUSED,
									   vString *buffer)
{
	tagFormat *format = getTagFormat (tag->kindIndex);
	const char *summaryFmt = (format == NULL || format->summaryFmt == NULL
							  || strlen (format->summaryFmt) == 0)
		? "%C" : format->summaryFmt;

	MIO *mio = mio_new_memory (NULL, 0, eRealloc, eFreeNoNullCheck);
	fmtPrint (fmtNew (summaryFmt), mio, tag);

	size_t size = 0;
	char *s = (char *)mio_memory_get_data (mio, &size);
	if (size > 0) vStringNCatS (buffer, s, size);

	mio_unref (mio);

	return vStringValue (buffer);
}

static fieldDefinition ExternFields [] = {
	{
		.name = "encodedName",
		.description = "encoded tag name",
		.render = renderFieldEncodedName,
		.enabled = false
	},
	{
		.name = "summary",
		.description = "summary line",
		.render = renderFieldSummary,
		.enabled = false
	}
};

static void makeExternTagEntry (const char *value, int kindIndex, int roleIndex,
								const char *pattern)
{
	tagEntryInfo tag;

	tag.kindIndex = KIND_GHOST_INDEX;

	if (roleIndex == ROLE_DEFINITION_INDEX)
		initTagEntry (&tag, value, kindIndex);
	else
	{
		langType language = getNamedLanguage ("Extern", 0);
		if (isLanguageRoleEnabled (language, kindIndex, roleIndex))
			initRefTagEntry (&tag, value, kindIndex, roleIndex);
	}

	tag.pattern = pattern;

	if (tag.kindIndex != KIND_GHOST_INDEX)
	{
		attachParserField (&tag, false,
						   ExternFields [F_ENCODEDNAME].ftype,
						   NULL);

		attachParserField (&tag, false,
						   ExternFields [F_SUMMARY].ftype,
						   NULL);

		makeTagEntry (&tag);
	}
}

static const char *makePattern (const char *string)
{
	char *pattern = NULL;

	if (string != NULL)
	{
		/* Convert string into a multi-line search pattern. */
		char searchChar = Option.backward ? '?' : '/';
		unsigned int patternLengthLimit = Option.patternLengthLimit;
		int extraLength = 0;
		size_t len = 0;

		/* Allocate enough memory to escape all characters. */
		pattern = xCalloc (strlen (string) * 2 + 3, char);
		pattern [len++] = searchChar;

		/* This is copied and modified from appendInputLine. */
		for (const char *p = string; *p != '\0'; ++p)
		{
			const char c = *p;

			if (patternLengthLimit != 0 && len > patternLengthLimit &&
				/* Do not cut inside a multi-byte UTF-8 character, but
				 * safe-guard it not to allow more than one extra valid UTF-8
				 * character in case it's not actually UTF-8.  To do that, limit
				 * to an extra 3 UTF-8 sub-bytes (0b10xxxxxx). */
				((((unsigned char) c) & 0xc0) != 0x80 || ++extraLength > 3))
				break;

			if (c == BACKSLASH || c == searchChar
				|| (c == '^' && len == 1)
				|| (c == '$' && (*(p + 1) == '\0'
								 || len == patternLengthLimit)))
			{
				/* Do not append an escaped character if the pattern length
				 * would exceed the limit. */
				if (len == patternLengthLimit)
					break;
				else
					pattern [len++] = BACKSLASH;
			}

			if (c == CRETURN || c == NEWLINE)
			{
				if (*(p + 1) == '\0')
					break;
				else
					pattern [len++] = SPACE;
			}
			else
				pattern [len++] = c;
		}

		pattern [len++] = searchChar;
		pattern [len] = '\0';
	}

	return pattern;
}

#if defined (WIN32) && !defined (__CYGWIN__)
static HANDLE pid;
#else
static int pid;
#endif
static FILE *outFile, *inFile;

static void initializeExternParser (void)
{
	if (parserCommand == NULL)
		error (FATAL, "No parser command");

#if defined (WIN32) && !defined (__CYGWIN__)
	SECURITY_ATTRIBUTES sa;

	sa.nLength = sizeof (sa);
	sa.bInheritHandle = TRUE;
	sa.lpSecurityDescriptor = NULL;

	HANDLE opipe [2], ipipe [2];

	if (CreatePipe (&opipe [0], &opipe [1], &sa, 0) == 0
		|| CreatePipe (&ipipe [0], &ipipe [1], &sa, 0) == 0)
		error (FATAL, "Cannot create pipe");

	SetHandleInformation (opipe [1], HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation (ipipe [0], HANDLE_FLAG_INHERIT, 0);

	STARTUPINFO si;

	ZeroMemory (&si, sizeof (si));
	si.cb = sizeof (si);
	si.hStdInput = opipe [0];
	si.hStdOutput = ipipe [1];
	si.hStdError = GetStdHandle (STD_ERROR_HANDLE);
	si.dwFlags = STARTF_USESTDHANDLES;

	PROCESS_INFORMATION pi;

	if (CreateProcess (NULL, parserCommand, NULL, NULL, TRUE, 0, NULL, NULL,
					   &si, &pi) == 0)
		error (FATAL, "Cannot execute %s", parserCommand);

	CloseHandle (pi.hThread);

	pid = pi.hProcess;

	outFile = fdopen (_open_osfhandle ((intptr_t)opipe [1], _O_WRONLY), "w");
	inFile = fdopen (_open_osfhandle ((intptr_t)ipipe [0], _O_RDONLY), "r");

	if (inFile == NULL || outFile == NULL)
		error (FATAL, "Cannot open stream");

	CloseHandle (opipe [0]);
	CloseHandle (ipipe [1]);
#else
	int ipipe [2], opipe [2];

	if (pipe (ipipe) < 0 || pipe (opipe) < 0)
		error (FATAL, "Cannot create pipe (%s)", strerror (errno));

	if ((pid = fork ()) < 0)
		error (FATAL, "Cannot create child process (%s)", strerror (errno));

	if (pid == 0)
	{

		if (dup2 (opipe [0], STDIN_FILENO) < 0
			|| dup2 (ipipe [1], STDOUT_FILENO) < 0)
			error (FATAL, "Cannot duplicate file descriptor (%s)",
				   strerror (errno));

		close (opipe [0]);
		close (opipe [1]);
		close (ipipe [0]);
		close (ipipe [1]);

		execlp (parserCommand, parserCommand, NULL);
		error (FATAL, "Cannot execute %s (%s)",
			   parserCommand, strerror (errno));
	}

	outFile = fdopen (opipe [1], "w");
	inFile = fdopen (ipipe [0], "r");

	if (inFile == NULL || outFile == NULL)
		error (FATAL, "Cannot open stream");

	close (opipe [0]);
	close (ipipe [1]);
#endif
}

static void findExternTags (void)
{
	if (outFile == NULL) initializeExternParser ();

	/* Write the input file name to the external parser. */
	char *fileName = (char *)getInputFileName ();

	if (isAbsolutePath (fileName))
	{
		if (CurrentDirectory == NULL)
			setCurrentDirectory ();

		fileName = relativeFilename (fileName, CurrentDirectory);
	}

	fputs (fileName, outFile);
	putc ('\n', outFile);
	fflush (outFile);

	/* Get the parser output. */
	json_error_t jerror;
	json_t *json = json_loadf (inFile, JSON_DISABLE_EOF_CHECK, &jerror);

	if (json_is_array (json))
	{
		/* The value of the variable xrefFormat overrides the value of the
		 * _xformat command line option.  For GNU Global, it should be set to
		 * "%R %-16{Extern.encodedName} %-10z %4n %-16F %{Extern.summary}". */
		if (xrefFormat != NULL)
		{
			if (Option.customXfmt) fmtDelete (Option.customXfmt);
			Option.customXfmt = fmtNew (xrefFormat);
		}

		int idx;
		json_t *jobject;
		size_t array_size = json_array_size (json);
		struct sTag {
			const char *name;
			const char *kind;
			int line;
		};
		struct sTag tag [array_size];

		json_array_foreach (json, idx, jobject)
		{
			if (json_unpack (jobject, "{s:s,s:s,s:i}",
							 "name", &(tag [idx].name),
							 "kind", &(tag [idx].kind),
							 "line", &(tag [idx].line)) != 0)
				error (FATAL, "Cannot parse JSON object");
		}

		// Sort the tag array by line numbers.
		struct sTag tempTag;

		for (idx = 0; idx < array_size; ++idx)
		{
			for (int i = idx + 1; i < array_size; ++i)
			{
				if (tag [idx].line > tag [i].line)
				{
					tempTag = tag [idx];
					tag [idx] = tag [i];
					tag [i] = tempTag;
				}
			}
		}

		for (idx = 0; idx < array_size; ++idx)
		{
			/* Advance the input file to the current line. */
			while (getInputLineNumber () < tag [idx].line)
				if (readLineFromInputFile () == NULL)
					break;

			langType language = getNamedLanguage ("Extern", 0);
			const kindDefinition *kdef =
				getLanguageKindForName (language, tag [idx].kind);

			if (kdef != NULL)
			{
				makeExternTagEntry (tag [idx].name, kdef->id,
									countLanguageRoles (language, kdef->id) > 0
									? 0 : ROLE_DEFINITION_INDEX,
									makePattern (tag [idx].name));
			}
		}
	}
}

static void finalizeExternParser (const langType language CTAGS_ATTR_UNUSED,
								  bool initialized)
{
	if (parserCommand != NULL) eFree (parserCommand);

	tagFormat *format = tagFormatList;

	while (format != NULL)
	{
		tagFormat *f = format;
		format = format-> next;

		if (f->kind != NULL) eFree (f->kind);
		if (f->prefix != NULL) eFree (f->prefix);
		if (f->summaryFmt != NULL) eFree (f->summaryFmt);

		eFree (f);
	}

	if (xrefFormat != NULL) eFree (xrefFormat);

	if (outFile != NULL)
	{
		fclose (outFile);
		fclose (inFile);

#if defined (WIN32) && !defined (__CYGWIN__)
		WaitForSingleObject (pid, INFINITE);
		CloseHandle (pid);
#else
		while (waitpid (pid, NULL, 0) < 0 && errno == EINTR);
#endif
	}
}

extern parserDefinition *ExternParser (void)
{
	parserDefinition *const def = parserNew ("Extern");

	def->fieldTable = ExternFields;
	def->fieldCount = ARRAY_SIZE (ExternFields);

	def->paramTable = ExternParams;
	def->paramCount = ARRAY_SIZE (ExternParams);

	def->parser = findExternTags;
	def->finalize = finalizeExternParser;
	def->useCork = CORK_QUEUE;

	return def;
}
