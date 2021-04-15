#!/usr/bin/gawk -f
#
# makeman - make xhippo man page from texinfo file.
# Modified from xzgv's makeman.awk which is:
# Public domain by Russell Marks.
#
# Requires gawk (it uses `gensub' (gawk only?) and `[:blank:]' (POSIX)).
#
# Note that this *IS NOT* a general texinfo to man page converter.
# It only supports the bare minimum required by xzgv.texinfo.
# It could probably be generalised to cope with most stuff though.

# BTW, I'm serious about that "bare minimum" bit - if I use more stuff
# in xzgv.texinfo, this is likely to need fixing to cope with it. :-)


BEGIN \
  {
  last_was_blank=1
  exit_val=0
  
  print ".\\\" *** Auto-generated by makeman, *edits will be lost*! ***"
  print ".\\\""
  print ".\\\""
  }


function do_header()
{
# man page header

print ".TH XHIPPO 1 \"" var_value["UPDATED"] "\" " \
	"\"Version " var_value["VERSION"] "\" \"Audio Software\""
print ".SH NAME"
print "xhippo \\- A GTK-based playlist manager for various UNIX sound players."
print ".\\\""

print ".SH SYNOPSIS"
print "xhippo [\\fIoptions\\fR] [\\fIdir\\fR | \\fIfile\\fR ...]"
}

END \
  {
  print ".SH AUTHOR"
  print "Adam Sampson <ats@offog.org> and others;"
  print "see the section \\fBChangeLog\\fR for details."
  print ".PP"
  print ".SH \"SEE ALSO\""
  print ".BR mpg123 \"(1),\""
  print ".BR ogg123 \"(1),\""
  print ".BR bplay \"(1), \" xmp \"(1),\""
  print ".BR mtvp \"(5), \" s3mod \"(5), \" tracker \"(5),\""
  print ".BR madplay \"(5)\""
  
  # should be last thing
  exit exit_val
  }



function add_line_to_para(line)
  {
  if(para=="")
    { para=line }
  else
    { para=para "\n" line }
  }


# output para, splitting at LFs as we go.
# This isn't actually required - we could just output the paragraph
# as one huge line. But that would be a bit unpleasant.
#
# Doing it nicely does, however, mean makeman takes ~50% longer to
# run, which is a pain. :-(
#
function output_para(	lhs)
  {
  while(para ~ /\n/)
    {
    lhs=gensub(/^([^\n]*)\n.*/,  "\\1", "g", para)
    para=gensub(/^[^\n]*\n(.*)/, "\\1", "g", para)
    
    # if it begins with .[^A-Z], escape the dot so troff ignores it.
    # The assumption is that this catches things like ".xvpics",
    # while leaving things like ".PP" alone.
    # The \f. bit is so it catches them after a font-change too
    # (curiously, dot-commands seem to count even after that!).
    # We have to allow .br though, which complicates it. :-/
    #
    # We assume no lines start with ' (which isn't handled here).
    #
    if(lhs ~ /^(\\f.)?\.[^A-Z]/ && lhs !~ /^\.br/)
      {
      # it's such fun escaping a dot in troff :-(((
      # @-cmds have been dealt with by now, so /^@/ should be unlikely.
      print ".cc @\n" lhs "\n@cc ."
      }
    else
      print lhs
    }
  
  # XXX ugly duplication here, should restructure so I don't need this
  #
  lhs=para
  if(lhs!="")
    {
    if(lhs ~ /^(\\f.)?\.[^A-Z]/ && lhs !~ /^\.br/)
      {
      print ".cc @\n" lhs "\n@cc ."
      }
    else
      print lhs
    }
  }



# first, keep copy of previous line available.

{ prevline=curline_orig; curline_orig=$0 }


/@c(omment)?\>/ \
  {
  sub(/@c.*/,"")
  if($0=="")
    {
    # that's right, this *is* a revolting kludge... :-)
    # (it's a special-case hack to avoid gaps in @item/@itemx pairs where
    # there's a comment in-between the two)
    if(prevline ~ /^@item[^x]/) { last_was_blank=1 }
    next
    }
  }

/^\\input texinfo\>/	{ next }

/^@ifinfo\>/,/^@end ifinfo\>/ \
			{ next }

/^@ignore\>/,/^@end ignore\>/ \
			{ next }

/^@menu\>/,/^@end menu\>/ \
			{ next }

/^@author\>/		{ next }
/^@bye\>/		{ next }
/^@finalout\>/          { next }
/^@noindent\>/          { next }

/^@example\>/		{ in_example++; next }
/^@end example\>/ \
  {
  in_example--
  last_was_blank=1
  add_line_to_para(".PP")
  next
  }


# section headings
# we ignore the `unnumbered' ones, assuming they are indexes.
/^@(chapter|section|subsection|subsubsection)\>/ \
  {
  sec_name=gensub(/^@[a-z]*[[:blank:]]*([^[:blank:]]+)/, "\\1", "g")
  # fix some headings to make them man-page-like
  sub(/^Overview/,"DESCRIPTION",sec_name)
  sub(/^Invoking xhippo/,"OPTIONS",sec_name)
  
  # chapter/section headings are in caps and are main headings (SH),
  # subsection/subsubsection aren't, and are subheadings (SS).
  
  if(/^@(chapter|section)/)
    { print ".SH \"" toupper(sec_name) "\"" }
  else
    { print ".SS \"" sec_name "\"" }
  
  if(sec_name=="DESCRIPTION")
    {
    print "(NB: This man page is automagically generated from xhippo's"
    print "texinfo file, and so may look a bit odd."
    print "We apologise for the inconvenience. :-))"
    print ".PP"
    }
  next
  }

# various tables
/^@v?table\>/ \
  {
  table_lvl++; table_type[table_lvl]=$2
  first_item[table_lvl]=1
  is_itemize[table_lvl]=0; next
  }

/^@itemize\>/ \
  {
  table_lvl++; table_type[table_lvl]=$2
  first_item[table_lvl]=1
  is_itemize[table_lvl]=1; next
  }

/^@end (v?table||itemize)\>/ \
  {
  table_lvl--
  add_line_to_para(".RE")
  was_tableend=1
  next
  }

/^@itemx?\>/ \
  {
  itemx=/^@itemx\>/
  
  if(!table_lvl)
    {
    print "makeman:" NR ": @item outside of @table" >"/dev/stderr"
    exit_val=1
    next
    }
  
  if(is_itemize[table_lvl])
    { $0=table_type[table_lvl] "{}" }
  else
    {
    $0=gensub(/^@itemx?[[:blank:]]+(.*)/, \
    	(first_item[table_lvl]?"":".RE\n") \
        table_type[table_lvl] "{\\1}\n.RS", 1)
    }
  
  first_item[table_lvl]=0
  
  # cope with there not being any blank lines between last item's text
  # and next @item. The regexp checks the last line wasn't an @item.
  #
  if(!last_was_blank && prevline !~ /^@itemx?\>/) { was_item=1 }
  }


/^@set\>/ \
  {
  valname=$2
  var_value[valname]=$3
  
  i=4
  while($(i)!="")
    {
    var_value[valname]=var_value[valname] " " $(i)
    i++
    }
  
  next
  }


/^@titlepage\>/ \
  {
  # given the way xhippo.texi is laid out, we know VERSION and
  # UPDATED must be set now.
  do_header()
  }

# ...but ignore the titlepage stuff apart from that.
/^@titlepage\>/,/^@end titlepage\>/ \
			{ next }

# these ones are ignored
/^@[ckfv]index\>/	{ last_was_blank=1;next }
/^@contents\>/		{ next }
/^@dircategory\>/	{ next }
/^@direntry\>/		{ next }
/^@end\>/		{ next }
/^@node\>/		{ next }
/^@page\>/		{ next }
/^@printindex\>/	{ next }
/^@setchapternewpage\>/	{ next }
/^@setfilename\>/	{ next }
/^@settitle\>/		{ next }
/^@subtitle\>/		{ next }
/^@title\>/		{ next }
/^@top\>/		{ next }
/^@unnumbered\>/	{ next }
/^@vskip\>/		{ next }

(/^$/ && !last_was_blank) || was_item || was_tableend \
  {
  # before printing the paragraph, fix attribute-ish stuff (e.g. @samp{foo}).
  
  # XXX nasty special-case hack to handle @-cmd within @-cmd :-(
  # @var{dir|file|integer} -> italic dir|file
  para=gensub(/@var\{(dir|file|integer)\}/, "\\\\fI\\1\\\\fR", "g", para)
  
  # the one footnote used at the time of writing isn't very
  # important (it's in the File Format Depths node), and I generally
  # try to avoid footnotes as they're not handled that well in Info,
  # so we just drop any footnotes.
  para=gensub(/@footnote\{.*\}/, "", "g", para)
  
  # @value
  while(/@value\{/)
    {
    valname=gensub(/.*@value\{([^}]+)\}.*/, "\\1", 0, para)
    para=gensub(/@value\{([^}]+)\}/, var_value[valname], 0, para)
    }
  
  # @{samp,file,var,cite,emph,url} -> italic
  para=gensub(/@(samp|file|var|cite|emph|url)\{([^}]+)\}/,
  	"\\\\fI\\2\\\\fR", "g", para)
  
  # @email{foo@@bar} -> <foo@bar>
  # (this assumes there's an @@ in it!)
  para=gensub(/@email\{(.*)@@([^}]+)\}/, "<\\1@\\2>", "g", para)
  
  # @strong -> bold
  para=gensub(/@strong\{([^}]+)\}/, "\\\\fB\\1\\\\fR", "g", para)
  
  # @{code,kbd} -> bold in quotes
  para=gensub(/@(code|kbd)\{([^}]+)\}/, "`\\\\fB\\2\\\\fR'", "g", para)
  
  # @bullet -> bold `o'
  # XXX this is crap :-)
  para=gensub(/@bullet\{([^}]*)\}/, "\\\\fBo\\\\fR \\1", "g", para)
  
  # @minus -> bold `-'
  # XXX also crap...
  para=gensub(/@minus\{([^}]*)\}/, "\\\\fB-\\\\fR \\1", "g", para)
  
  # @asis -> normal
  para=gensub(/@asis\{([^}]*)\}/, "\\1", "g", para)
  
  # @dots{} -> ...
  para=gensub(/@dots\{\}/, "...", "g", para)
  
  
  # hairy hack to support five-arg form of @pxref
  para=gensub(/@pxref\{([^,]+),([^,]*),([^,]*),([^,]+),([^,]*)\}/, \
  	"see \\\\fB\\1\\\\fR in the \\\\fI\\4\\\\fR info file", "g", para)
  
  # XXX none of @pxref/@xref/@ref uppercase their args yet :-/
  
  # @pxref{foo} -> see FOO (with FOO in bold)
  para=gensub(/@pxref\{([^}]+)\}/, \
  	"see \\\\fB\\1\\\\fR", "g", para)
  
  # @xref{foo} -> See FOO (with FOO in bold)
  para=gensub(/@xref\{([^}]+)\}/, \
  	"See \\\\fB\\1\\\\fR", "g", para)
  
  # @ref{foo} -> FOO (with FOO in bold)
  para=gensub(/@ref\{([^}]+)\}/, \
  	"\\\\fB\\1\\\\fR", "g", para)
  
  
  # finally, a bit of a kludge - replace "\fBInvocation\fR" with
  # "\fBOptions\fR", assuming it's in a @pxref/@xref/@ref.
  para=gensub(/\\fBInvocation\\fR/, "\\\\fBOptions\\\\fR", "g", para)
  
  # if it still contains { or }, we probably missed something - say so.
  if(para ~ /[\{\}]/)
    {
    print "makeman:" NR ": warning: output paragraph contains a brace" \
    	>"/dev/stderr"
    exit_val=1
    }
  
  output_para()
  para=""
  
  print ".PP"
  last_was_blank=1
  if(was_item) { add_line_to_para($0) }
  was_item=0
  was_tableend=0
  next
  }

/^[^@]/ || /^@(file|samp|code|var|cite|emph|strong)/	{ last_was_blank=0 }

# otherwise...
!/^$/ \
  {
  # if there's one we haven't handled, complain (but keep going).
  # This only checks for start-of-line ones; the {/} check in the
  # paragraph-output routine should catch any others.
  
  if(/^@[a-z]+\>[^\{]/)
    {
    print "makeman:" NR ": unhandled texinfo command in this paragraph!" \
    	>"/dev/stderr"
    exit_val=1
    }
  
  add_line_to_para($0)
  if(in_example)
    {
    add_line_to_para(".br")
    }
  }