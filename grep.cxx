#include "application.hxx"
#include "sourceFile.hxx"

void Application::grep (const GrepArgs & args, std::ostream & cout) {
  Json::FastWriter writer;

  const auto refs = storage_.grep (args.usr);
  auto ref = refs.begin ();
  const auto end = refs.end ();
  for ( ; ref != end ; ++ref ) {
    Json::Value json = ref->json();
    SourceFile file (ref->file);
    json["lineContents"] = file.line (ref->line1);
    cout << writer.write (json);
  }

  if(args.find_overridens)
  {
      const auto overridenRefDefs = storage_.findOverridenDefinition(args.usr);
      for (auto it = overridenRefDefs.begin() ; it != overridenRefDefs.end(); it++) {
          Json::Value json = it->json();
          SourceFile file (it->file);
          json["lineContents"] = file.line (it->line1);
          cout << writer.write (json);
      }
  }
}
