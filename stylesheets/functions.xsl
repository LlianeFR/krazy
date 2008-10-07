<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" 
  xmlns:xsd="http://www.w3.org/2001/XMLSchema"
  xmlns:ebn="http://www.englishbreakfastnetwork.org/krazy" version="2.0">
  
  <xsl:import href="globalvars.xsl" />

  <!--
    - Returns the number of checkers run for given fileType.
    -
    - @p fileType The fileType for which to count the number of checkers run.
    -             Give 'all' when you want the total number of checkers run.
    -->
  <xsl:function name="ebn:checkerCount" as="xsd:integer">
    <xsl:param name="fileType" as="xsd:string" />
    <xsl:sequence 
      select="if ($fileType eq 'all')
              then count( $global.doc/krazy/file-types/file-type/check )
              else count( $global.doc/krazy/file-types/file-type[@value=$fileType]/check )" />
  </xsl:function>
  
  <!--
    - Returns the number of files which have issues.
    -
    - @p fileType The fileType for which to count the number of files having
    -             issues. Give 'all' to get the total number of files which have
    -             issues.
    -->
  <xsl:function name="ebn:numberOfFilesWithIssues" as="xsd:integer">
    <xsl:param name="fileType" as="xsd:string" />
    <xsl:sequence 
      select="if ($fileType eq 'all')
              then count( $global.doc/krazy/file-types/file-type/check/file )
              else count( $global.doc/krazy/file-types/file-type[@value=$fileType]/check/file )" />
  </xsl:function>

  <xsl:function name="ebn:issueCount" as="xsd:integer">
    <xsl:param name="fileType" as="xsd:string" />
    <xsl:sequence 
      select="if ($fileType eq 'all')
              then count( $global.doc/krazy/file-types/file-type/check/file/issues/line )
              else count( $global.doc/krazy/file-types/file-type[@value=$fileType]/check/file/issues/line )" />
  </xsl:function>

  <!--
    - Returns the total number of files processed by krazy.
    -->
  <xsl:function name="ebn:processedFilesCount" as="xsd:integer">
    <xsl:value-of select="$global.doc/krazy/global/processed-files/@value" />
  </xsl:function>
  
  <!--
    - Returns the date on which the krazy report was generated.
    -->
  <xsl:function name="ebn:dateOfRun" as="xsd:string">
    <xsl:value-of select="$global.doc/krazy/global/date/@value" />
  </xsl:function>
</xsl:stylesheet>

<!-- kate:space-indent on; -->