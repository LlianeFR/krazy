/*
    Copyright (C) 2009 Bertjan Broeksema <b.broeksema@kdemail.net>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "document.h"

#include <parser/AST.h>
#include <parser/Control.h>
#include <parser/DiagnosticClient.h>
#include <parser/Literals.h>
#include <parser/Scope.h>
#include <parser/Semantic.h>
#include <parser/TranslationUnit.h>

#include <preprocessor/PreprocessorClient.h>

#include <QtCore/QByteArray>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QtDebug>

using namespace CPlusPlus;
using namespace CppModel;

namespace {

  class DocumentDiagnosticClient : public DiagnosticClient
  {
    enum { MAX_MESSAGE_COUNT = 10 };

    public:
      DocumentDiagnosticClient(Document *doc, QList<DiagnosticMessage> *messages)
        : doc(doc),
          messages(messages)
      {}

      virtual void report(int level,
                          StringLiteral *fileId,
                          unsigned line, unsigned column,
                          const char *format, va_list ap)
      {
        if (messages->count() == MAX_MESSAGE_COUNT)
          return;

        QString const fileName = QString::fromUtf8(fileId->chars(), fileId->size());

        if (fileName != doc->fileName())
          return;

        QString message;
        message.vsprintf(format, ap);

        DiagnosticMessage m(convertLevel(level), doc->fileName(),
                            line, column, message);
        messages->append(m);
      }

      static DiagnosticMessage::Level convertLevel(int level) 
      {
        switch (level) 
        {
          case Warning: return DiagnosticMessage::Warning;
          case Error:   return DiagnosticMessage::Error;
          case Fatal:   return DiagnosticMessage::Fatal;
          default:      return DiagnosticMessage::Error;
        }
      }

      Document *doc;
      QList<DiagnosticMessage> *messages;
  };

} // anonymous namespace

/// Implementation of Include

Document::Include::Include(Ptr const &document, Client::IncludeType type, unsigned line)
  : m_document(document), m_line(line), m_type(type)
{}

Document::Ptr Document::Include::document() const
{
  return m_document;
}

unsigned Document::Include::line() const
{
  return m_line;
}

Client::IncludeType Document::Include::type() const
{
  return m_type;
}

/// Implementation of Document

/// Document :: public functions

Document::~Document()
{
  delete m_translationUnit;
  delete m_control->diagnosticClient();
  delete m_control;
}

QString Document::absoluteFileName() const
{
  if (m_path.endsWith(QDir::separator()))
    return m_path + m_fileName;
  else
    return m_path + QDir::separator() + m_fileName;
}

QList<Macro> Document::definedMacros() const
{
  return m_definedMacros;
}

QList<DiagnosticMessage> Document::diagnosticMessages() const
{
  return m_diagnosticMessages;
}

QString Document::fileName() const
{
  return m_fileName;
}

QList<Document::Include> Document::includes() const
{
  return m_includes;
}

QList<MacroUse> Document::macroUses() const
{
  return m_macroUses;
}

QList<CharBlock> Document::skippedBlocks() const
{
  return m_skippedBlocks;
}

TranslationUnit * Document::translationUnit() const
{
  return m_translationUnit;
}

/// Document :: Protected functions

void Document::addDiagnosticMessage(DiagnosticMessage const &message)
{
  m_diagnosticMessages.append(message);
}

void Document::addIncludeFile(Ptr const &includedDocument
                            , Client::IncludeType type
                            , unsigned line)
{
  m_includes.append(Include(includedDocument, type, line));
}

void Document::addMacroUse(Macro const &macro
                         , unsigned offset
                         , unsigned length
                         , QVector<MacroArgumentReference> const &actuals)
{
  MacroUse use(macro, offset, offset + length);

  foreach (const MacroArgumentReference &actual, actuals) {
    const CharBlock arg(actual.position(), actual.position() + actual.length());

    use.addArgument(arg);
  }

  m_macroUses.append(use);
}

void Document::appendMacro(Macro const &macro)
{
  m_definedMacros.append(macro);
}

void Document::check(QSharedPointer<Namespace> globalNamespace)
{
  if (globalNamespace)
    m_globalNamespace = globalNamespace;
  else
    m_globalNamespace = QSharedPointer<Namespace>(m_control->newNamespace(0));

  // Check the included documents.
  foreach(Include inc, m_includes)
    inc.document()->check(globalNamespace);

  if (! m_translationUnit->ast())
    return; // nothing to do.

  // Now check this document.
  Semantic semantic(m_control);
  Scope *globals = m_globalNamespace->members();

  if (TranslationUnitAST *ast = m_translationUnit->ast()->asTranslationUnit())
    for (DeclarationAST *decl = ast->declarations; decl; decl = decl->next)
      semantic.check(decl, globals);
}

Document::Ptr Document::create(QString const &fileName)
{
  Document::Ptr doc(new Document(fileName));
  return doc;
}

void Document::setPath(QString const &path)
{
  m_path = path;
}

void Document::setSource(QByteArray const &source)
{
  // NOTE: We don't store the actual source at all in the document object. From
  //       the source we create directly an unannotated AST.
  m_translationUnit->setSource(source.constBegin(), source.size());
  m_translationUnit->tokenize();
  m_translationUnit->parse(TranslationUnit::ParseTranlationUnit);
  //m_translationUnit->release();
}

void Document::startSkippingBlocks(unsigned start)
{
  m_skippedBlocks.append(CharBlock(start, 0));
}

void Document::stopSkippingBlocks(unsigned stop)
{
  unsigned start = m_skippedBlocks.back().begin();
  if (start > stop)
    m_skippedBlocks.removeLast(); // Ignore this block, it's invalid.
  else
    m_skippedBlocks.back() = CharBlock(start, stop);
}

/// Document :: Private functions

Document::Document(const QString &fileName)
  : m_fileName(fileName)
  , m_control(new Control())
{
  m_control->setDiagnosticClient(new DocumentDiagnosticClient(this, &m_diagnosticMessages));

  const QByteArray localFileName = fileName.toUtf8();
  StringLiteral *fileId = m_control->findOrInsertFileName(localFileName.constData(),
                                                          localFileName.size());
  m_translationUnit = new TranslationUnit(m_control, fileId);
  m_translationUnit->setQtMocRunEnabled(true);
  m_translationUnit->setObjCEnabled(true);

  (void) m_control->switchTranslationUnit(m_translationUnit);
}