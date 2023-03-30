.. _ctags-externalparser(7):

==============================================================
ctags-externalparser
==============================================================

The Universal Ctags Extern parser

:Version: 6.0.0
:Manual group: Universal Ctags
:Manual section: 7

SYNOPSIS
--------
|	**ctags** ... --languages=+Extern ...
|	**ctags** ... --language-force=Extern ...
|	**ctags** ... --map-Extern=+... ...

DESCRIPTION
-----------
The Universal Ctags Extern parser generates tags from the output of an external
parser.  The output of the external parser must be a sequence of JSON formatted
arrays of tag objects.  Each tag object must contain entries for the name, the
kind and the source line number of the tag:

.. code-block:: JSON

	{ "name": "<tag>", "kind": "<kind>", "line": <line> }

A tag object can have more entries, but only ``name``, ``kind`` and ``line`` are
required by the Universal Ctags Extern parser.

OPTIONS
-------
The Universal Ctags Extern parser has three parser-specific options:

``param-Extern.parser=<cmd>``
	Set the command name of the external parser to *<cmd>*.  See "`The
	External Parser`_".

``param-Extern.kinds=<map>[,<map>[...]]``
	Define parser-specific kinds by a comma-separated list of mappings
	*<map>*.  See "`Kinds`_".

``param-Extern.xformat=<fmt>``
	Set the Xref output format string to *<fmt>*.  See "`The Xref Output
	Format`_".

The External Parser
~~~~~~~~~~~~~~~~~~~
The value of the ``--param-Extern.parser`` option is the command name of an
external parser.  The external parser must be able to read file names from the
standard input, one file name per line, and print JSON formatted arrays of tag
objects to the standard output, one array per file.

Kinds
~~~~~
The value of the ``--param-Extern.kinds`` option is a comma-separated list of
mappings:

.. code-block:: text

	<kind>:<letter>:<role>:<prefix>:<summaryfmt>

Each mapping defines a kind with long-name flag ``<kind>`` and one-letter flag
``<letter>``.  ``<role>`` is one of ``def``, ``ref`` or ``other``.  If it is
``def``, ``<kind>`` is a kind for definition tags.  If it is ``ref`` or
``other``, ``<kind>`` is a kind for reference tags with role ``ref`` or
``other``, respectively.  See "`Roles`_".  ``<prefix>`` is a prefix string for
the tag names.  See "`Prefixed Tag Names`_".  ``<summaryfmt>`` is a format
string for the ``summary`` field.  See "`The summary Field`_".

The Universal Ctags Extern parser generates tags only from those tag objects
whose kinds are defined by the ``--param-Extern.kinds`` option.  Other tag
objects are ignored by the Universal Ctags Extern parser.

The Xref Output Format
~~~~~~~~~~~~~~~~~~~~~~
The ``--param-Extern.xformat`` option can be used to customize the Xref output
format when the Universal Ctags Extern parser is used as a plugin parser for the
GNU Global source code tagging system (https://www.gnu.org/software/global).
GNU Global sets the Xref output format string to a fixed value by calling
ctags with the command line option ``--_xformat="%R %-16N %4n
%-16F %C"``.  The command line option is hard-coded in the GNU Global source
code and cannot be superseded by another ``--_xformat`` command line option.
With the Universal Ctags Extern parser, however, it can be superseded by the
``--param-Extern.xformat`` option.  For example, calling ctags
with the option

.. code-block:: sh

	--param-Extern:xformat="%R %-16{Extern.encodedName} %4n %-16F %{Extern.summary}"

replaces the tag names by the values of the ``encodedName`` field and the
compact input lines by the values of the ``summary`` field in the Xref output.
The ``encodedName`` field and the ``summary`` field are parser-specific fields
of the Universal Ctags Extern parser.  See "`Encoded Tag Names`_" and "`The
summary Field`_".

Roles
~~~~~
Two roles are defined by the Universal Ctags Extern parser, ``ref`` and
``other``.  They can be attached to kinds by the ``--param-Extern.kinds``
option.  See "`Kinds`_".  The ``ref`` role is meant to be attached to kinds of
reference tags that reference definition tags and the ``other`` role is meant to
be attached to kinds of reference tags that do not reference definition tags.

The distinction between the different kinds of reference tags is borrowed from
the GNU Global source code tagging system (https://www.gnu.org/software/global).
GNU Global distinguishes between definition tags, reference tags and "other
symbols".  In GNU Global, reference tags are tags that reference definition tags
and "other symbols" are tags that are neither definition tags nor reference
tags.  In Universal Ctags, different kinds of reference tags can be
distinguished by their roles.  However, when using the Universal Ctags Extern
parser as a plugin parser for GNU Global, attaching the ``other`` role to the
kinds of reference tags that do not reference definition tags is pure
conventionally.  In GNU Global, the distinction between reference tags and
"other symbols" depends only on whether definition tags are referenced or not.

Prefixed Tag Names
~~~~~~~~~~~~~~~~~~
The GNU Global source code tagging system (https://www.gnu.org/software/global)
can distinguish between only three kinds of tags, definition tags, reference
tags and "other symbols".  The Universal Ctags Extern parser can distinguish
between more than three kinds of tags, namely between all kinds that are defined
by the ``--param-Extern.kinds`` option.  When the Universal Ctags Extern parser
is used as a plugin parser for GNU Global, it may be desirable not to lose this
distinction.  Therefore, the tag names can be prefixed by kind-specific prefix
strings in the output of the Universal Ctags Extern parser.  Different kinds of
tags can then be distinguished by the tag names if their prefix strings differ.
For example, if the prefix string for a given kind of definition tags is ``_``,
differing from the prefix strings of all other kinds of definition tags, all GNU
Global tags for that kind can be listed by the GNU Global command ``global -c
_``.

The prefix strings can be attached to kinds by the ``param-Extern.kinds``
option.  See "`Kinds`_".  To ensure that the prefix strings are not altered by
percent-encoding, they must contain only printable 7-bit ASCII characters except
the percent character.  See "`Encoded Tag Names`".

Fields
~~~~~~
The two parser-specific fields ``encodedName`` and ``summary`` are implemented
by the Universal Ctags Extern parser.

Encoded Tag Names
.................
The GNU Global source code tagging system (https://www.gnu.org/software/global)
does not allow tag names with spaces, and tag names with non-ASCII characters
produce warnings in GNU Global.  To use the Universal Ctags Extern parser as a
plugin parser for GNU Global with tag names that contain spaces or non-ASCII
characters, the Universal Ctags Extern parser has implemented an ``encodedName``
field, whose values are the percent-encoded tag names.  The ``encodeName`` field
can be used as a replacement for the ``name`` field in the output of Universal
Ctags.  For example, while the standard Xref output format string is ``"%-16N
%4n %-16F %C"``, calling ctags with the command line option

.. code-block:: sh

	--_xformat="%-16{Extern.encodedName} %4n %-16F %C"

replaces the tag names by percent-encoded tag names in the Xref output.  In
particular, spaces are replaced by ``%20``.  Leading exclamation marks of tag
names are also percent-encoded, because otherwise the tags could be confused
with Universal Ctags pseudo tags.

Before percent-encoding, the tag name is prefixed by the prefix string, which is
assigned to the kind of the tag by the ``param-Extern.kinds`` option.  See
"`Kinds`_" and "`Prefixed Tag Names`_".  Therefore, the leading character of a
percent-encoded tag name whose kind-specific prefix string is empty and whose
beginning is equal to the kind-specific prefix string of another kind is also
percent-encoded, because otherwise it could be confused with a prefixed tag
name.

The summary Field
.................
Sometimes the value of the compact input line field ``C`` does not contain any
useful information.  As a replacement for the compact input line field, the
Universal Ctags Extern parser has implemented a configurable ``summary`` field.
For example, while the standard Xref output format string is ``"%-16N %4n %-16F
%C"``, calling ctags with the command line option

.. code-block:: sh

	--_xformat="%-16N %4n %-16F %{Extern.summary}"

replaces the compact input lines by the values of the ``summary`` field in the
Xref output.  The value of the ``summary`` field can be configured, depending on
the kind of the tag.  It is the string to which the summary format string that
is attached to the kind of the tag by the ``--param-Extern.kinds`` option
resolves.  See "`Kinds`_".  If no summary format string is attached to the kind
of the tag, the value of the ``summary`` field is the value of the compact input
line field ``C``.

When the Universal Ctags Extern parser is used as a plugin parser for GNU
Global, the summary field is recognized only for definition tags and ignored for
reference tags and "other symbols".

EXAMPLE
-------
A suitable external parser for the Universal Ctags Extern parser is
``remark-tags`` (https://github.com/bernardjoseph/remark-tags>), a plugin for
the ``remark`` Markdown parser (https://remark.js.org>).  With ``remark-tags``
as external parser, the Universal Ctags Extern parser can generate tags for
Markdown files.  Say we have the following Markdown file, ``example.md``, with
YAML frontmatter (https://pandoc.org/MANUAL.html#extension-yaml_metadata_block),
``link`` inline directives
(https://talk.commonmark.org/t/generic-directives-plugins-syntax/444) and
Pandoc-style citations (https://pandoc.org/MANUAL.html#citations):

.. code-block:: Markdown

	---
	title: An Example document
	author: N. N.
	date: 2021-10-01
	keywords:
		- Markdown
		- Tags
	nocite: |
		@doe1999, @smith2004
	---

	See :link[A link directive]{ref=Target}, :link[Another target]
	and @wadler1989 [sec. 1.3; and -@hughes1990, pp. 4].

The ``remark-tags`` parser is configured as follows:

.. code-block:: JSON

	{
		"plugins": {
			"/usr/local/lib/node_modules/@bernardjoseph/remark-tags/index.js": {
				"kinds": {
					"yaml": {
						"title": {"kind": "title"},
						"author": {"kind": "author"},
						"date": {"kind": "date"},
						"keywords": {"kind": "keyword"},
						"nocite": {"kind": "nocite", "nocite": true}
					},
					"textDirective": {
						"link": {"kind": "link", "ref": "ref"}
					},
					"citekeyId": {"kind": "cite"}
				}
			}
		}
	}

and the Universal Ctags Extern parser is configured as follows:

.. code-block:: sh

	# Set the external parser command to remark-tags-filter.
	--param-Extern.parser=remark-tags-filter

	# Define title, author, date, keyword, nocite, link and cite kinds and prefixes.
	--param-Extern.kinds=title:t:def::%N (%K),author:a:other:_:%N (%K),date:d:other:_:%N (%K),keyword:k:other:_:%N (%K),nocite:o:other:_:%N (%K),link:l:ref::,cite:c:other:_:

	# Configure the Xref output.
	--param-Extern.xformat=%R %-16{Extern.encodedName} %4n %-16F %{Extern.summary}

Then, running

.. code-block:: sh

	echo example.md | ctags --language-force=Extern --extras=+r -xu --filter

yields

.. code-block:: console

	D An%20Example%20document    1 example.md       An Example document (title)
	R _N.%20N.            1 example.md       N. N. (author)
	R _2021-10-01         1 example.md       2021-10-01 (date)
	R _Markdown           1 example.md       Markdown (keyword)
	R _Tags               1 example.md       Tags (keyword)
	R _doe1999            1 example.md       doe1999 (nocite)
	R _smith2004          1 example.md       smith2004 (nocite)
	R Target             12 example.md       See :link[A link directive]{ref=Target}, :link[Another target]
	R Another%20target   12 example.md       See :link[A link directive]{ref=Target}, :link[Another target]
	R _wadler1989        13 example.md       and @wadler1989 [sec. 1.3; and -@hughes1990, pp. 4].
	R _hughes1990        13 example.md       and @wadler1989 [sec. 1.3; and -@hughes1990, pp. 4].

SEE ALSO
--------
:ref:`ctags(1) <ctags(1)>`, :ref:`ctags-client-tools(7) <ctags-client-tools(7)>`
