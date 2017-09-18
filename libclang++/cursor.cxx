#include "cursor.hxx"
#include "translationUnit.hxx"
#include "sourceLocation.hxx"

namespace LibClang {
  Cursor::Cursor (CXCursor raw)
    : cursor_ (raw)
  { }

  Cursor::Cursor (const TranslationUnit & tu)
    : cursor_ (clang_getTranslationUnitCursor (tu.raw()))
  { }

  Cursor::Cursor (const TranslationUnit & tu,
          const std::string & fileName, const unsigned int offset)
    : cursor_ (clang_getCursor
               (tu.raw(),
                clang_getLocationForOffset (tu.raw(),
                                            clang_getFile (tu.raw(), fileName.c_str()),
                                            offset)))
  { }

  const CXCursor & Cursor::raw () const { return cursor_; };

  bool Cursor::isNull () const {
    return clang_Cursor_isNull (raw());
  }

  bool Cursor::isVirtual() const{
      bool isVirtual = clang_CXXMethod_isVirtual(raw()) != 0;
      bool isPureVirtual = clang_CXXMethod_isPureVirtual(raw()) != 0;
      bool isVirtualBase = clang_isVirtualBase(raw()) != 0;
      return isVirtual || isPureVirtual || isVirtualBase;
  }

  std::vector<std::string> Cursor::getAllOverridenMethods()const
  {
      std::vector<std::string> retCursors;
      if(!isVirtual())
      {
          return retCursors;
      }
      CXCursor *ret = NULL;
      unsigned num = 0;
      clang_getOverriddenCursors(raw(), &ret, &num);
      
      for(unsigned i = 0; i < num; i++)
      {
          Cursor curCursor = Cursor(*(ret + i));
          clang_disposeOverriddenCursors(ret+i);
          retCursors.push_back(curCursor.USR());
      }
      return retCursors;
  }

  bool Cursor::isUnexposed () const {
    return clang_isUnexposed (clang_getCursorKind(raw()));
  }

  bool Cursor::isDeclaration () const {
    return clang_isDeclaration(clang_getCursorKind(raw()));
  }

  Cursor Cursor::referenced () const {
    return clang_getCursorReferenced (raw());
  }

  std::string Cursor::kindStr () const {
    CXCursorKind kind = clang_getCursorKind (raw());
    CXString kindSpelling = clang_getCursorKindSpelling (kind);
    std::string res = clang_getCString (kindSpelling);
    clang_disposeString (kindSpelling);
    return res;
  }

  std::string Cursor::spelling () const {
    CXString cursorSpelling = clang_getCursorSpelling (raw());
    std::string res = clang_getCString (cursorSpelling);
    clang_disposeString (cursorSpelling);
    return res;
  }

  std::string Cursor::USR () const {
    CXString cursorUSR = clang_getCursorUSR (raw());
    std::string res = clang_getCString (cursorUSR);
    clang_disposeString (cursorUSR);
    return res;
  }

  SourceLocation Cursor::location () const {
    return clang_getCursorLocation (raw());
  }

  SourceLocation Cursor::end () const {
    CXSourceRange extent = clang_getCursorExtent (raw());
    return clang_getRangeEnd (extent);
  }

}
