# Loki RPC Documentation Generator
A self contained tool that only depends on the C++ standard library to auto-generate documentation for RPC commands used in the Loki daemon and wallet.

## Building
Use the supplied build scripts `build_msvc.bat` (for Visual Studio) or `build_gcc.sh` (for GCC), or simply invoke the compiler on the single CPP file.
The build script compiles the executable to the Bin folder at the root of this repository.

## Usage
For linux a helper script is provided to copy over the currently known set of files that are required for documentation generation.
`./copy_source_files <path to loki root directory>`

The executable outputs the contents to standard output. Redirect this to your file of choice, i.e.
`loki_rpc_doc_generator.exe <cpp header file> > generated_document.html`

## Markup
```
#define LOKI_RPC_DOC_INTROSPECT

LOKI_RPC_DOC_INTROSPECT
namespace cryptonote
{
    LOKI_RPC_DOC_INTROSPECT
    struct Helper
    {
        std::vector<int> bar;      // An array of ints
        internal_struct internal_; // @NoLokiRPCDocGen This will not be serialized to markdown
    };
}

LOKI_RPC_DOC_INTROSPECT
// Comment which is the description of the RPC command
struct I_AM_RPC_COMMAND
{
    struct request_t
    {
        int hello;      // Add a comment which gets pulled into the markdown as the description of the variable
        Heleper helper; // We support struct declarations and recurse through members (if they are also marked up)
    };

    struct response_t
    {
        int hello; // Add a comment which gets pulled into the markdown as the description of the variable
    };
}
```
