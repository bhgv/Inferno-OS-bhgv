# Native attach root Qid

`styxattachroot(server, pending, qid)` selects a native-authorized file or directory
root for the current attach transaction. Return0 means staged, -1 means refused.
Pass pending=nil only inside the synchronous attach callback. A deferred socket
owner passes its live caller-owned StyxAttach before styxattachreply. Selection
alone emits no wire reply and grants no authority. Backend admission, metadata,
open and I/O callbacks remain responsible for permissions and the selected node.

Default behavior remains directory Qroot. Explicit roots preserve the full64-bit
Qid and its type in both Rattach and the allocated fid. Authentication Qids and
conflicts with cached file/directory types refuse. Choose a distinct Qid path for
a callback-backed file rather than reusing the server's cached directory Qroot.
No message or codec extension is introduced. Zero-element Twalk clones the file
fid normally; walking descendants of a file refuses through normal dispatch.

Call only on the native socket owner. Pending transactions cancelled by flush,
reset, disconnect or shutdown cannot be retargeted or completed. Original
synchronous and deferred directory-root clients are unchanged. The real-codec
deferred-attach oracle checks synchronous/deferred file roots,64-bit identity,
file fid cloning, descendant refusal and inactive setters.

Native host может выбрать file root до ответа attach. Callback использует pending=nil,
отложенный owner — живой StyxAttach. По умолчанию остаётся directory Qroot.
Формат Styx не меняется, права по-прежнему проверяет выбранный backend.
