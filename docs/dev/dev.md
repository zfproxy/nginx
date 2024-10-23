开发指南

## 开发指南

<table width="100%"><tbody><tr><td align="left"><a href="#introduction">Introduction</a><br><a href="#code_layout">Code layout</a><br><a href="#include_files">Include files</a><br><a href="#integers">Integers</a><br><a href="#common_return_codes">Common return codes</a><br><a href="#error_handling">Error handling</a><br><a href="#strings">Strings</a><br><a href="#string_overview">Overview</a><br><a href="#formatting">Formatting</a><br><a href="#numeric_conversion">Numeric conversion</a><br><a href="#regex">Regular expressions</a><br><a href="#time">Time</a><br><a href="#containers">Containers</a><br><a href="#array">Array</a><br><a href="#list">List</a><br><a href="#queue">Queue</a><br><a href="#red_black_tree">Red-Black tree</a><br><a href="#hash">Hash</a><br><a href="#memory_management">Memory management</a><br><a href="#heap">Heap</a><br><a href="#pool">Pool</a><br><a href="#shared_memory">Shared memory</a><br><a href="#logging">Logging</a><br><a href="#cycle">Cycle</a><br><a href="#buffer">Buffer</a><br><a href="#networking">Networking</a><br><a href="#connection">Connection</a><br><a href="#events">Events</a><br><a href="#event">Event</a><br><a href="#i_o_events">I/O events</a><br><a href="#timer_events">Timer events</a><br><a href="#posted_events">Posted events</a><br><a href="#event_loop">Event loop</a><br><a href="#processes">Processes</a><br><a href="#threads">Threads</a><br><a href="#Modules">Modules</a><br><a href="#adding_new_modules">Adding new modules</a><br><a href="#core_modules">Core Modules</a><br><a href="#config_directives">Configuration Directives</a><br><a href="#http">HTTP</a><br><a href="#http_connection">Connection</a><br><a href="#http_request">Request</a><br><a href="#http_conf">Configuration</a><br><a href="#http_phases">Phases</a><br><a href="#http_variables">Variables</a><br><a href="#http_complex_values">Complex values</a><br><a href="#http_request_redirection">Request redirection</a><br><a href="#http_subrequests">Subrequests</a><br><a href="#http_request_finalization">Request finalization</a><br><a href="#http_request_body">Request body</a><br><a href="#http_request_body_filters">Request body filters</a><br><a href="#http_response">Response</a><br><a href="#http_response_body">Response body</a><br><a href="#http_response_body_filters">Response body filters</a><br><a href="#http_building_filter_modules">Building filter modules</a><br><a href="#http_body_buffers_reuse">Buffer reuse</a><br><a href="#http_load_balancing">Load balancing</a><br><a href="#examples">Examples</a><br><a href="#code_style">Code style</a><br><a href="#code_style_general_rules">General rules</a><br><a href="#code_style_files">Files</a><br><a href="#code_style_comments">Comments</a><br><a href="#code_style_preprocessor">Preprocessor</a><br><a href="#code_style_types">Types</a><br><a href="#code_style_variables">Variables</a><br><a href="#code_style_functions">Functions</a><br><a href="#code_style_expressions">Expressions</a><br><a href="#code_style_conditionals_and_loops">Conditionals and Loops</a><br><a href="#code_style_labels">Labels</a><br><a href="#debug_memory">Debugging memory issues</a><br><a href="#common_pitfals">Common Pitfalls</a><br><a href="#module_pitfall">Writing a C module</a><br><a href="#c_strings">C Strings</a><br><a href="#global_variables">Global Variables</a><br><a href="#manual_memory_management">Manual Memory Management</a><br><a href="#threads_pitfalls">Threads</a><br><a href="#libraries">Blocking Libraries</a><br><a href="#http_requests_to_ext">HTTP Requests to External Services</a><br></td></tr></tbody></table>

#### Introduction

#### Code layout

+   `auto` — Build scripts
+   `src`
    +   `core` — Basic types and functions — string, array, log, pool, etc.
    +   `event` — Event core
        +   `modules` — Event notification modules: `epoll`, `kqueue`, `select` etc.
    +   `http` — Core HTTP module and common code
        +   `modules` — Other HTTP modules
        +   `v2` — HTTP/2
    +   `mail` — Mail modules
    +   `os` — Platform-specific code
        +   `unix`
        +   `win32`
    +   `stream` — Stream modules

#### Include files

The following two `#include` statements must appear at the beginning of every nginx file:

#include <ngx\_config.h>
#include <ngx\_core.h>

In addition to that, HTTP code should include

> #include <ngx\_http.h>

Mail code should include

> #include <ngx\_mail.h>

Stream code should include

> #include <ngx\_stream.h>

#### Integers

For general purposes, nginx code uses two integer types, `ngx_int_t` and `ngx_uint_t`, which are typedefs for `intptr_t` and `uintptr_t` respectively.

#### Common return codes

Most functions in nginx return the following codes:

+   `NGX_OK` — Operation succeeded.
+   `NGX_ERROR` — Operation failed.
+   `NGX_AGAIN` — Operation incomplete; call the function again.
+   `NGX_DECLINED` — Operation rejected, for example, because it is disabled in the configuration. This is never an error.
+   `NGX_BUSY` — Resource is not available.
+   `NGX_DONE` — Operation complete or continued elsewhere. Also used as an alternative success code.
+   `NGX_ABORT` — Function was aborted. Also used as an alternative error code.

#### Error handling

The `ngx_errno` macro returns the last system error code. It's mapped to `errno` on POSIX platforms and to `GetLastError()` call in Windows. The `ngx_socket_errno` macro returns the last socket error number. Like the `ngx_errno` macro, it's mapped to `errno` on POSIX platforms. It's mapped to the `WSAGetLastError()` call on Windows. Accessing the values of `ngx_errno` or `ngx_socket_errno` more than once in a row can cause performance issues. If the error value might be used multiple times, store it in a local variable of type `ngx_err_t`. To set errors, use the `ngx_set_errno(errno)` and `ngx_set_socket_errno(errno)` macros.

The values of `ngx_errno` and `ngx_socket_errno` can be passed to the logging functions `ngx_log_error()` and `ngx_log_debugX()`, in which case system error text is added to the log message.

Example using `ngx_errno`:

> ngx\_int\_t
> ngx\_my\_kill(ngx\_pid\_t pid, ngx\_log\_t \*log, int signo)
> {
>     ngx\_err\_t  err;
> 
>     if (kill(pid, signo) == -1) {
>         err = ngx\_errno;
> 
>         ngx\_log\_error(NGX\_LOG\_ALERT, log, err, "kill(%P, %d) failed", pid, signo);
> 
>         if (err == NGX\_ESRCH) {
>             return 2;
>         }
> 
>         return 1;
>     }
> 
>     return 0;
> }

#### Strings

#### Overview

For C strings, nginx uses the unsigned character type pointer `u_char *`.

The nginx string type `ngx_str_t` is defined as follows:

> typedef struct {
>     size\_t      len;
>     u\_char     \*data;
> } ngx\_str\_t;

The `len` field holds the string length and `data` holds the string data. The string, held in `ngx_str_t`, may or may not be null-terminated after the `len` bytes. In most cases it’s not. However, in certain parts of the code (for example, when parsing configuration), `ngx_str_t` objects are known to be null-terminated, which simplifies string comparison and makes it easier to pass the strings to syscalls.

The string operations in nginx are declared in `src/core/ngx_string.h` Some of them are wrappers around standard C functions:

+   `ngx_strcmp()`
+   `ngx_strncmp()`
+   `ngx_strstr()`
+   `ngx_strlen()`
+   `ngx_strchr()`
+   `ngx_memcmp()`
+   `ngx_memset()`
+   `ngx_memcpy()`
+   `ngx_memmove()`

Other string functions are nginx-specific

+   `ngx_memzero()` — Fills memory with zeroes.
+   `ngx_explicit_memzero()` — Does the same as `ngx_memzero()`, but this call is never removed by the compiler's dead store elimination optimization. This function can be used to clear sensitive data such as passwords and keys.
+   `ngx_cpymem()` — Does the same as `ngx_memcpy()`, but returns the final destination address This one is handy for appending multiple strings in a row.
+   `ngx_movemem()` — Does the same as `ngx_memmove()`, but returns the final destination address.
+   `ngx_strlchr()` — Searches for a character in a string, delimited by two pointers.

The following functions perform case conversion and comparison:

+   `ngx_tolower()`
+   `ngx_toupper()`
+   `ngx_strlow()`
+   `ngx_strcasecmp()`
+   `ngx_strncasecmp()`

The following macros simplify string initialization:

+   `ngx_string(text)` — static initializer for the `ngx_str_t` type from the C string literal `text`
+   `ngx_null_string` — static empty string initializer for the `ngx_str_t` type
+   `ngx_str_set(str, text)` — initializes string `str` of `ngx_str_t *` type with the C string literal `text`
+   `ngx_str_null(str)` — initializes string `str` of `ngx_str_t *` type with the empty string

#### Formatting

The following formatting functions support nginx-specific types:

+   `ngx_sprintf(buf, fmt, ...)`
+   `ngx_snprintf(buf, max, fmt, ...)`
+   `ngx_slprintf(buf, last, fmt, ...)`
+   `ngx_vslprintf(buf, last, fmt, args)`
+   `ngx_vsnprintf(buf, max, fmt, args)`

The full list of formatting options, supported by these functions is in `src/core/ngx_string.c`. Some of them are:

+   `%O` — `off_t`
+   `%T` — `time_t`
+   `%z` — `ssize_t`
+   `%i` — `ngx_int_t`
+   `%p` — `void *`
+   `%V` — `ngx_str_t *`
+   `%s` — `u_char *` (null-terminated)
+   `%*s` — `size_t + u_char *`

You can prepend `u` on most types to make them unsigned. To convert output to hex, use `X` or `x`.

For example:

> u\_char      buf\[NGX\_INT\_T\_LEN\];
> size\_t      len;
> ngx\_uint\_t  n;
> 
> /\* set n here \*/
> 
> len = ngx\_sprintf(buf, "%ui", n) — buf;

#### Numeric conversion

Several functions for numeric conversion are implemented in nginx. The first four each convert a string of given length to a positive integer of the indicated type. They return `NGX_ERROR` on error.

+   `ngx_atoi(line, n)` — `ngx_int_t`
+   `ngx_atosz(line, n)` — `ssize_t`
+   `ngx_atoof(line, n)` — `off_t`
+   `ngx_atotm(line, n)` — `time_t`

There are two additional numeric conversion functions. Like the first four, they return `NGX_ERROR` on error.

+   `ngx_atofp(line, n, point)` — Converts a fixed point floating number of given length to a positive integer of type `ngx_int_t`. The result is shifted left by `point` decimal positions. The string representation of the number is expected to have no more than `points` fractional digits. For example, `ngx_atofp("10.5", 4, 2)` returns `1050`.
+   `ngx_hextoi(line, n)` — Converts a hexadecimal representation of a positive integer to `ngx_int_t`.

#### Regular expressions

The regular expressions interface in nginx is a wrapper around the [PCRE](http://www.pcre.org) library. The corresponding header file is `src/core/ngx_regex.h`.

To use a regular expression for string matching, it first needs to be compiled, which is usually done at the configuration phase. Note that since PCRE support is optional, all code using the interface must be protected by the surrounding `NGX_PCRE` macro:

> #if (NGX\_PCRE)
> ngx\_regex\_t          \*re;
> ngx\_regex\_compile\_t   rc;
> 
> u\_char                errstr\[NGX\_MAX\_CONF\_ERRSTR\];
> 
> ngx\_str\_t  value = ngx\_string("message (\\\\d\\\\d\\\\d).\*Codeword is '(?<cw>\\\\w+)'");
> 
> ngx\_memzero(&rc, sizeof(ngx\_regex\_compile\_t));
> 
> rc.pattern = value;
> rc.pool = cf->pool;
> rc.err.len = NGX\_MAX\_CONF\_ERRSTR;
> rc.err.data = errstr;
> /\* rc.options can be set to NGX\_REGEX\_CASELESS \*/
> 
> if (ngx\_regex\_compile(&rc) != NGX\_OK) {
>     ngx\_conf\_log\_error(NGX\_LOG\_EMERG, cf, 0, "%V", &rc.err);
>     return NGX\_CONF\_ERROR;
> }
> 
> re = rc.regex;
> #endif

After successful compilation, the `captures` and `named_captures` fields in the `ngx_regex_compile_t` structure contain the count of all captures and named captures, respectively, found in the regular expression.

The compiled regular expression can then be used for matching against strings:

> ngx\_int\_t  n;
> int        captures\[(1 + rc.captures) \* 3\];
> 
> ngx\_str\_t input = ngx\_string("This is message 123. Codeword is 'foobar'.");
> 
> n = ngx\_regex\_exec(re, &input, captures, (1 + rc.captures) \* 3);
> if (n >= 0) {
>     /\* string matches expression \*/
> 
> } else if (n == NGX\_REGEX\_NO\_MATCHED) {
>     /\* no match was found \*/
> 
> } else {
>     /\* some error \*/
>     ngx\_log\_error(NGX\_LOG\_ALERT, log, 0, ngx\_regex\_exec\_n " failed: %i", n);
> }

The arguments to `ngx_regex_exec()` are the compiled regular expression `re`, the string to match `input`, an optional array of integers to hold any `captures` that are found, and the array's `size`. The size of the `captures` array must be a multiple of three, as required by the [PCRE API](http://www.pcre.org/original/doc/html/pcreapi.html). In the example, the size is calculated from the total number of captures plus one for the matched string itself.

If there are matches, captures can be accessed as follows:

> u\_char     \*p;
> size\_t      size;
> ngx\_str\_t   name, value;
> 
> /\* all captures \*/
> for (i = 0; i < n \* 2; i += 2) {
>     value.data = input.data + captures\[i\];
>     value.len = captures\[i + 1\] — captures\[i\];
> }
> 
> /\* accessing named captures \*/
> 
> size = rc.name\_size;
> p = rc.names;
> 
> for (i = 0; i < rc.named\_captures; i++, p += size) {
> 
>     /\* capture name \*/
>     name.data = &p\[2\];
>     name.len = ngx\_strlen(name.data);
> 
>     n = 2 \* ((p\[0\] << 8) + p\[1\]);
> 
>     /\* captured value \*/
>     value.data = &input.data\[captures\[n\]\];
>     value.len = captures\[n + 1\] — captures\[n\];
> }

The `ngx_regex_exec_array()` function accepts the array of `ngx_regex_elt_t` elements (which are just compiled regular expressions with associated names), a string to match, and a log. The function applies expressions from the array to the string until either a match is found or no more expressions are left. The return value is `NGX_OK` when there is a match and `NGX_DECLINED` otherwise, or `NGX_ERROR` in case of error.

#### Time

The `ngx_time_t` structure represents time with three separate types for seconds, milliseconds, and the GMT offset:

> typedef struct {
>     time\_t      sec;
>     ngx\_uint\_t  msec;
>     ngx\_int\_t   gmtoff;
> } ngx\_time\_t;

The `ngx_tm_t` structure is an alias for `struct tm` on UNIX platforms and `SYSTEMTIME` on Windows.

To obtain the current time, it is usually sufficient to access one of the available global variables, representing the cached time value in the desired format.

The available string representations are:

+   `ngx_cached_err_log_time` — Used in error log entries: `"1970/09/28 12:00:00"`
+   `ngx_cached_http_log_time` — Used in HTTP access log entries: `"28/Sep/1970:12:00:00 +0600"`
+   `ngx_cached_syslog_time` — Used in syslog entries: `"Sep 28 12:00:00"`
+   `ngx_cached_http_time` — Used in HTTP headers: `"Mon, 28 Sep 1970 06:00:00 GMT"`
+   `ngx_cached_http_log_iso8601` — The ISO 8601 standard format: `"1970-09-28T12:00:00+06:00"`

The `ngx_time()` and `ngx_timeofday()` macros return the current time value in seconds and are the preferred way to access the cached time value.

To obtain the time explicitly, use `ngx_gettimeofday()`, which updates its argument (pointer to `struct timeval`). The time is always updated when nginx returns to the event loop from system calls. To update the time immediately, call `ngx_time_update()`, or `ngx_time_sigsafe_update()` if updating the time in the signal handler context.

The following functions convert `time_t` into the indicated broken-down time representation. The first function in each pair converts `time_t` to `ngx_tm_t` and the second (with the `_libc_` infix) to `struct tm`:

+   `ngx_gmtime(), ngx_libc_gmtime()` — Time expressed as UTC
+   `ngx_localtime(), ngx_libc_localtime()` — Time expressed relative to the local time zone

The `ngx_http_time(buf, time)` function returns a string representation suitable for use in HTTP headers (for example, `"Mon, 28 Sep 1970 06:00:00 GMT"`). The `ngx_http_cookie_time(buf, time)` returns a string representation function returns a string representation suitable for HTTP cookies (`"Thu, 31-Dec-37 23:55:55 GMT"`).

#### Containers

#### Array

The nginx array type `ngx_array_t` is defined as follows

> typedef struct {
>     void        \*elts;
>     ngx\_uint\_t   nelts;
>     size\_t       size;
>     ngx\_uint\_t   nalloc;
>     ngx\_pool\_t  \*pool;
> } ngx\_array\_t;

The elements of the array are available in the `elts` field. The `nelts` field holds the number of elements. The `size` field holds the size of a single element and is set when the array is initialized.

Use the `ngx_array_create(pool, n, size)` call to create an array in a pool, and the `ngx_array_init(array, pool, n, size)` call to initialize an array object that has already been allocated.

> ngx\_array\_t  \*a, b;
> 
> /\* create an array of strings with preallocated memory for 10 elements \*/
> a = ngx\_array\_create(pool, 10, sizeof(ngx\_str\_t));
> 
> /\* initialize string array for 10 elements \*/
> ngx\_array\_init(&b, pool, 10, sizeof(ngx\_str\_t));

Use the following functions to add elements to an array:

+   `ngx_array_push(a)` adds one tail element and returns pointer to it
+   `ngx_array_push_n(a, n)` adds `n` tail elements and returns pointer to the first one

If the currently allocated amount of memory is not large enough to accommodate the new elements, a new block of memory is allocated and the existing elements are copied to it. The new memory block is normally twice as large as the existing one.

> s = ngx\_array\_push(a);
> ss = ngx\_array\_push\_n(&b, 3);

#### List

In nginx a list is a sequence of arrays, optimized for inserting a potentially large number of items. The `ngx_list_t` list type is defined as follows:

> typedef struct {
>     ngx\_list\_part\_t  \*last;
>     ngx\_list\_part\_t   part;
>     size\_t            size;
>     ngx\_uint\_t        nalloc;
>     ngx\_pool\_t       \*pool;
> } ngx\_list\_t;

The actual items are stored in list parts, which are defined as follows:

> typedef struct ngx\_list\_part\_s  ngx\_list\_part\_t;
> 
> struct ngx\_list\_part\_s {
>     void             \*elts;
>     ngx\_uint\_t        nelts;
>     ngx\_list\_part\_t  \*next;
> };

Before use, a list must be initialized by calling `ngx_list_init(list, pool, n, size)` or created by calling `ngx_list_create(pool, n, size)`. Both functions take as arguments the size of a single item and a number of items per list part. To add an item to a list, use the `ngx_list_push(list)` function. To iterate over the items, directly access the list fields as shown in the example:

> ngx\_str\_t        \*v;
> ngx\_uint\_t        i;
> ngx\_list\_t       \*list;
> ngx\_list\_part\_t  \*part;
> 
> list = ngx\_list\_create(pool, 100, sizeof(ngx\_str\_t));
> if (list == NULL) { /\* error \*/ }
> 
> /\* add items to the list \*/
> 
> v = ngx\_list\_push(list);
> if (v == NULL) { /\* error \*/ }
> ngx\_str\_set(v, "foo");
> 
> v = ngx\_list\_push(list);
> if (v == NULL) { /\* error \*/ }
> ngx\_str\_set(v, "bar");
> 
> /\* iterate over the list \*/
> 
> part = &list->part;
> v = part->elts;
> 
> for (i = 0; /\* void \*/; i++) {
> 
>     if (i >= part->nelts) {
>         if (part->next == NULL) {
>             break;
>         }
> 
>         part = part->next;
>         v = part->elts;
>         i = 0;
>     }
> 
>     ngx\_do\_smth(&v\[i\]);
> }

Lists are primarily used for HTTP input and output headers.

Lists do not support item removal. However, when needed, items can internally be marked as missing without actually being removed from the list. For example, to mark HTTP output headers (which are stored as `ngx_table_elt_t` objects) as missing, set the `hash` field in `ngx_table_elt_t` to zero. Items marked in this way are explicitly skipped when the headers are iterated over.

#### Queue

In nginx a queue is an intrusive doubly linked list, with each node defined as follows:

> typedef struct ngx\_queue\_s  ngx\_queue\_t;
> 
> struct ngx\_queue\_s {
>     ngx\_queue\_t  \*prev;
>     ngx\_queue\_t  \*next;
> };

The head queue node is not linked with any data. Use the `ngx_queue_init(q)` call to initialize the list head before use. Queues support the following operations:

+   `ngx_queue_insert_head(h, x)`, `ngx_queue_insert_tail(h, x)` — Insert a new node
+   `ngx_queue_remove(x)` — Remove a queue node
+   `ngx_queue_split(h, q, n)` — Split a queue at a node, returning the queue tail in a separate queue
+   `ngx_queue_add(h, n)` — Add a second queue to the first queue
+   `ngx_queue_head(h)`, `ngx_queue_last(h)` — Get first or last queue node
+   `ngx_queue_sentinel(h)` - Get a queue sentinel object to end iteration at
+   `ngx_queue_data(q, type, link)` — Get a reference to the beginning of a queue node data structure, considering the queue field offset in it

An example:

> typedef struct {
>     ngx\_str\_t    value;
>     ngx\_queue\_t  queue;
> } ngx\_foo\_t;
> 
> ngx\_foo\_t    \*f;
> ngx\_queue\_t   values, \*q;
> 
> ngx\_queue\_init(&values);
> 
> f = ngx\_palloc(pool, sizeof(ngx\_foo\_t));
> if (f == NULL) { /\* error \*/ }
> ngx\_str\_set(&f->value, "foo");
> 
> ngx\_queue\_insert\_tail(&values, &f->queue);
> 
> /\* insert more nodes here \*/
> 
> for (q = ngx\_queue\_head(&values);
>      q != ngx\_queue\_sentinel(&values);
>      q = ngx\_queue\_next(q))
> {
>     f = ngx\_queue\_data(q, ngx\_foo\_t, queue);
> 
>     ngx\_do\_smth(&f->value);
> }

#### Red-Black tree

The `src/core/ngx_rbtree.h` header file provides access to the effective implementation of red-black trees.

> typedef struct {
>     ngx\_rbtree\_t       rbtree;
>     ngx\_rbtree\_node\_t  sentinel;
> 
>     /\* custom per-tree data here \*/
> } my\_tree\_t;
> 
> typedef struct {
>     ngx\_rbtree\_node\_t  rbnode;
> 
>     /\* custom per-node data \*/
>     foo\_t              val;
> } my\_node\_t;

To deal with a tree as a whole, you need two nodes: root and sentinel. Typically, they are added to a custom structure, allowing you to organize your data into a tree in which the leaves contain a link to or embed your data.

To initialize a tree:

> my\_tree\_t  root;
> 
> ngx\_rbtree\_init(&root.rbtree, &root.sentinel, insert\_value\_function);

To traverse a tree and insert new values, use the "`insert_value`" functions. For example, the `ngx_str_rbtree_insert_value` function deals with the `ngx_str_t` type. Its arguments are pointers to a root node of an insertion, the newly created node to be added, and a tree sentinel.

> void ngx\_str\_rbtree\_insert\_value(ngx\_rbtree\_node\_t \*temp,
>                                  ngx\_rbtree\_node\_t \*node,
>                                  ngx\_rbtree\_node\_t \*sentinel)

The traversal is pretty straightforward and can be demonstrated with the following lookup function pattern:

> my\_node\_t \*
> my\_rbtree\_lookup(ngx\_rbtree\_t \*rbtree, foo\_t \*val, uint32\_t hash)
> {
>     ngx\_int\_t           rc;
>     my\_node\_t          \*n;
>     ngx\_rbtree\_node\_t  \*node, \*sentinel;
> 
>     node = rbtree->root;
>     sentinel = rbtree->sentinel;
> 
>     while (node != sentinel) {
> 
>         n = (my\_node\_t \*) node;
> 
>         if (hash != node->key) {
>             node = (hash < node->key) ? node->left : node->right;
>             continue;
>         }
> 
>         rc = compare(val, node->val);
> 
>         if (rc < 0) {
>             node = node->left;
>             continue;
>         }
> 
>         if (rc > 0) {
>             node = node->right;
>             continue;
>         }
> 
>         return n;
>     }
> 
>     return NULL;
> }

The `compare()` function is a classic comparator function that returns a value less than, equal to, or greater than zero. To speed up lookups and avoid comparing user objects that can be big, an integer hash field is used.

To add a node to a tree, allocate a new node, initialize it and call `ngx_rbtree_insert()`:

>     my\_node\_t          \*my\_node;
>     ngx\_rbtree\_node\_t  \*node;
> 
>     my\_node = ngx\_palloc(...);
>     init\_custom\_data(&my\_node->val);
> 
>     node = &my\_node->rbnode;
>     node->key = create\_key(my\_node->val);
> 
>     ngx\_rbtree\_insert(&root->rbtree, node);

To remove a node, call the `ngx_rbtree_delete()` function:

> ngx\_rbtree\_delete(&root->rbtree, node);

#### Hash

Hash table functions are declared in `src/core/ngx_hash.h`. Both exact and wildcard matching are supported. The latter requires extra setup and is described in a separate section below.

Before initializing a hash, you need to know the number of elements it will hold so that nginx can build it optimally. Two parameters that need to be configured are `max_size` and `bucket_size`, as detailed in a separate [document](../hash.html). They are usually configurable by the user. Hash initialization settings are stored with the `ngx_hash_init_t` type, and the hash itself is `ngx_hash_t`:

> ngx\_hash\_t       foo\_hash;
> ngx\_hash\_init\_t  hash;
> 
> hash.hash = &foo\_hash;
> hash.key = ngx\_hash\_key;
> hash.max\_size = 512;
> hash.bucket\_size = ngx\_align(64, ngx\_cacheline\_size);
> hash.name = "foo\_hash";
> hash.pool = cf->pool;
> hash.temp\_pool = cf->temp\_pool;

The `key` is a pointer to a function that creates the hash integer key from a string. There are two generic key-creation functions: `ngx_hash_key(data, len)` and `ngx_hash_key_lc(data, len)`. The latter converts a string to all lowercase characters, so the passed string must be writable. If that is not true, pass the `NGX_HASH_READONLY_KEY` flag to the function, initializing the key array (see below).

The hash keys are stored in `ngx_hash_keys_arrays_t` and are initialized with `ngx_hash_keys_array_init(arr, type)`: The second parameter (`type`) controls the amount of resources preallocated for the hash and can be either `NGX_HASH_SMALL` or `NGX_HASH_LARGE`. The latter is appropriate if you expect the hash to contain thousands of elements.

> ngx\_hash\_keys\_arrays\_t  foo\_keys;
> 
> foo\_keys.pool = cf->pool;
> foo\_keys.temp\_pool = cf->temp\_pool;
> 
> ngx\_hash\_keys\_array\_init(&foo\_keys, NGX\_HASH\_SMALL);

To insert keys into a hash keys array, use the `ngx_hash_add_key(keys_array, key, value, flags)` function:

> ngx\_str\_t k1 = ngx\_string("key1");
> ngx\_str\_t k2 = ngx\_string("key2");
> 
> ngx\_hash\_add\_key(&foo\_keys, &k1, &my\_data\_ptr\_1, NGX\_HASH\_READONLY\_KEY);
> ngx\_hash\_add\_key(&foo\_keys, &k2, &my\_data\_ptr\_2, NGX\_HASH\_READONLY\_KEY);

To build the hash table, call the `ngx_hash_init(hinit, key_names, nelts)` function:

> ngx\_hash\_init(&hash, foo\_keys.keys.elts, foo\_keys.keys.nelts);

The function fails if `max_size` or `bucket_size` parameters are not big enough.

When the hash is built, use the `ngx_hash_find(hash, key, name, len)` function to look up elements:

> my\_data\_t   \*data;
> ngx\_uint\_t   key;
> 
> key = ngx\_hash\_key(k1.data, k1.len);
> 
> data = ngx\_hash\_find(&foo\_hash, key, k1.data, k1.len);
> if (data == NULL) {
>     /\* key not found \*/
> }

#### Wildcard matching

To create a hash that works with wildcards, use the `ngx_hash_combined_t` type. It includes the hash type described above and has two additional keys arrays: `dns_wc_head` and `dns_wc_tail`. The initialization of basic properties is similar to a regular hash:

> ngx\_hash\_init\_t      hash
> ngx\_hash\_combined\_t  foo\_hash;
> 
> hash.hash = &foo\_hash.hash;
> hash.key = ...;

It is possible to add wildcard keys using the `NGX_HASH_WILDCARD_KEY` flag:

> /\* k1 = ".example.org"; \*/
> /\* k2 = "foo.\*";        \*/
> ngx\_hash\_add\_key(&foo\_keys, &k1, &data1, NGX\_HASH\_WILDCARD\_KEY);
> ngx\_hash\_add\_key(&foo\_keys, &k2, &data2, NGX\_HASH\_WILDCARD\_KEY);

The function recognizes wildcards and adds keys into the corresponding arrays. Please refer to the [map](../http/ngx_http_map_module.html#map) module documentation for the description of the wildcard syntax and the matching algorithm.

Depending on the contents of added keys, you may need to initialize up to three key arrays: one for exact matching (described above), and two more to enable matching starting from the head or tail of a string:

> if (foo\_keys.dns\_wc\_head.nelts) {
> 
>     ngx\_qsort(foo\_keys.dns\_wc\_head.elts,
>               (size\_t) foo\_keys.dns\_wc\_head.nelts,
>               sizeof(ngx\_hash\_key\_t),
>               cmp\_dns\_wildcards);
> 
>     hash.hash = NULL;
>     hash.temp\_pool = pool;
> 
>     if (ngx\_hash\_wildcard\_init(&hash, foo\_keys.dns\_wc\_head.elts,
>                                foo\_keys.dns\_wc\_head.nelts)
>         != NGX\_OK)
>     {
>         return NGX\_ERROR;
>     }
> 
>     foo\_hash.wc\_head = (ngx\_hash\_wildcard\_t \*) hash.hash;
> }

The keys array needs to be sorted, and initialization results must be added to the combined hash. The initialization of `dns_wc_tail` array is done similarly.

The lookup in a combined hash is handled by the `ngx_hash_find_combined(chash, key, name, len)`:

> /\* key = "bar.example.org"; — will match ".example.org" \*/
> /\* key = "foo.example.com"; — will match "foo.\*"        \*/
> 
> hkey = ngx\_hash\_key(key.data, key.len);
> res = ngx\_hash\_find\_combined(&foo\_hash, hkey, key.data, key.len);

#### Memory management

#### Heap

To allocate memory from system heap, use the following functions:

+   `ngx_alloc(size, log)` — Allocate memory from system heap. This is a wrapper around `malloc()` with logging support. Allocation error and debugging information is logged to `log`.
+   `ngx_calloc(size, log)` — Allocate memory from system heap like `ngx_alloc()`, but fill memory with zeros after allocation.
+   `ngx_memalign(alignment, size, log)` — Allocate aligned memory from system heap. This is a wrapper around `posix_memalign()` on those platforms that provide that function. Otherwise implementation falls back to `ngx_alloc()` which provides maximum alignment.
+   `ngx_free(p)` — Free allocated memory. This is a wrapper around `free()`

#### Pool

Most nginx allocations are done in pools. Memory allocated in an nginx pool is freed automatically when the pool is destroyed. This provides good allocation performance and makes memory control easy.

A pool internally allocates objects in continuous blocks of memory. Once a block is full, a new one is allocated and added to the pool memory block list. When the requested allocation is too large to fit into a block, the request is forwarded to the system allocator and the returned pointer is stored in the pool for further deallocation.

The type for nginx pools is `ngx_pool_t`. The following operations are supported:

+   `ngx_create_pool(size, log)` — Create a pool with specified block size. The pool object returned is allocated in the pool as well. The `size` should be at least `NGX_MIN_POOL_SIZE` and a multiple of `NGX_POOL_ALIGNMENT`.
+   `ngx_destroy_pool(pool)` — Free all pool memory, including the pool object itself.
+   `ngx_palloc(pool, size)` — Allocate aligned memory from the specified pool.
+   `ngx_pcalloc(pool, size)` — Allocate aligned memory from the specified pool and fill it with zeroes.
+   `ngx_pnalloc(pool, size)` — Allocate unaligned memory from the specified pool. Mostly used for allocating strings.
+   `ngx_pfree(pool, p)` — Free memory that was previously allocated in the specified pool. Only allocations that result from requests forwarded to the system allocator can be freed.

> u\_char      \*p;
> ngx\_str\_t   \*s;
> ngx\_pool\_t  \*pool;
> 
> pool = ngx\_create\_pool(1024, log);
> if (pool == NULL) { /\* error \*/ }
> 
> s = ngx\_palloc(pool, sizeof(ngx\_str\_t));
> if (s == NULL) { /\* error \*/ }
> ngx\_str\_set(s, "foo");
> 
> p = ngx\_pnalloc(pool, 3);
> if (p == NULL) { /\* error \*/ }
> ngx\_memcpy(p, "foo", 3);

Chain links (`ngx_chain_t`) are actively used in nginx, so the nginx pool implementation provides a way to reuse them. The `chain` field of `ngx_pool_t` keeps a list of previously allocated links ready for reuse. For efficient allocation of a chain link in a pool, use the `ngx_alloc_chain_link(pool)` function. This function looks up a free chain link in the pool list and allocates a new chain link if the pool list is empty. To free a link, call the `ngx_free_chain(pool, cl)` function.

Cleanup handlers can be registered in a pool. A cleanup handler is a callback with an argument which is called when pool is destroyed. A pool is usually tied to a specific nginx object (like an HTTP request) and is destroyed when the object reaches the end of its lifetime. Registering a pool cleanup is a convenient way to release resources, close file descriptors or make final adjustments to the shared data associated with the main object.

To register a pool cleanup, call `ngx_pool_cleanup_add(pool, size)`, which returns a `ngx_pool_cleanup_t` pointer to be filled in by the caller. Use the `size` argument to allocate context for the cleanup handler.

> ngx\_pool\_cleanup\_t  \*cln;
> 
> cln = ngx\_pool\_cleanup\_add(pool, 0);
> if (cln == NULL) { /\* error \*/ }
> 
> cln->handler = ngx\_my\_cleanup;
> cln->data = "foo";
> 
> ...
> 
> static void
> ngx\_my\_cleanup(void \*data)
> {
>     u\_char  \*msg = data;
> 
>     ngx\_do\_smth(msg);
> }

#### Shared memory

Shared memory is used by nginx to share common data between processes. The `ngx_shared_memory_add(cf, name, size, tag)` function adds a new shared memory entry `ngx_shm_zone_t` to a cycle. The function receives the `name` and `size` of the zone. Each shared zone must have a unique name. If a shared zone entry with the provided `name` and `tag` already exists, the existing zone entry is reused. The function fails with an error if an existing entry with the same name has a different tag. Usually, the address of the module structure is passed as `tag`, making it possible to reuse shared zones by name within one nginx module.

The shared memory entry structure `ngx_shm_zone_t` has the following fields:

+   `init` — Initialization callback, called after the shared zone is mapped to actual memory
+   `data` — Data context, used to pass arbitrary data to the `init` callback
+   `noreuse` — Flag that disables reuse of a shared zone from the old cycle
+   `tag` — Shared zone tag
+   `shm` — Platform-specific object of type `ngx_shm_t`, having at least the following fields:
    +   `addr` — Mapped shared memory address, initially NULL
    +   `size` — Shared memory size
    +   `name` — Shared memory name
    +   `log` — Shared memory log
    +   `exists` — Flag that indicates shared memory was inherited from the master process (Windows-specific)

Shared zone entries are mapped to actual memory in `ngx_init_cycle()` after the configuration is parsed. On POSIX systems, the `mmap()` syscall is used to create the shared anonymous mapping. On Windows, the `CreateFileMapping()`/ `MapViewOfFileEx()` pair is used.

For allocating in shared memory, nginx provides the slab pool `ngx_slab_pool_t` type. A slab pool for allocating memory is automatically created in each nginx shared zone. The pool is located in the beginning of the shared zone and can be accessed by the expression `(ngx_slab_pool_t *) shm_zone->shm.addr`. To allocate memory in a shared zone, call either `ngx_slab_alloc(pool, size)` or `ngx_slab_calloc(pool, size)`. To free memory, call `ngx_slab_free(pool, p)`.

Slab pool divides all shared zone into pages. Each page is used for allocating objects of the same size. The specified size must be a power of 2, and greater than the minimum size of 8 bytes. Nonconforming values are rounded up. A bitmask for each page tracks which blocks are in use and which are free for allocation. For sizes greater than a half page (which is usually 2048 bytes), allocation is done an entire page at a time

To protect data in shared memory from concurrent access, use the mutex available in the `mutex` field of `ngx_slab_pool_t`. A mutex is most commonly used by the slab pool while allocating and freeing memory, but it can be used to protect any other user data structures allocated in the shared zone. To lock or unlock a mutex, call `ngx_shmtx_lock(&shpool->mutex)` or `ngx_shmtx_unlock(&shpool->mutex)` respectively.

> ngx\_str\_t        name;
> ngx\_foo\_ctx\_t   \*ctx;
> ngx\_shm\_zone\_t  \*shm\_zone;
> 
> ngx\_str\_set(&name, "foo");
> 
> /\* allocate shared zone context \*/
> ctx = ngx\_pcalloc(cf->pool, sizeof(ngx\_foo\_ctx\_t));
> if (ctx == NULL) {
>     /\* error \*/
> }
> 
> /\* add an entry for 64k shared zone \*/
> shm\_zone = ngx\_shared\_memory\_add(cf, &name, 65536, &ngx\_foo\_module);
> if (shm\_zone == NULL) {
>     /\* error \*/
> }
> 
> /\* register init callback and context \*/
> shm\_zone->init = ngx\_foo\_init\_zone;
> shm\_zone->data = ctx;
> 
> 
> ...
> 
> 
> static ngx\_int\_t
> ngx\_foo\_init\_zone(ngx\_shm\_zone\_t \*shm\_zone, void \*data)
> {
>     ngx\_foo\_ctx\_t  \*octx = data;
> 
>     size\_t            len;
>     ngx\_foo\_ctx\_t    \*ctx;
>     ngx\_slab\_pool\_t  \*shpool;
> 
>     value = shm\_zone->data;
> 
>     if (octx) {
>         /\* reusing a shared zone from old cycle \*/
>         ctx->value = octx->value;
>         return NGX\_OK;
>     }
> 
>     shpool = (ngx\_slab\_pool\_t \*) shm\_zone->shm.addr;
> 
>     if (shm\_zone->shm.exists) {
>         /\* initialize shared zone context in Windows nginx worker \*/
>         ctx->value = shpool->data;
>         return NGX\_OK;
>     }
> 
>     /\* initialize shared zone \*/
> 
>     ctx->value = ngx\_slab\_alloc(shpool, sizeof(ngx\_uint\_t));
>     if (ctx->value == NULL) {
>         return NGX\_ERROR;
>     }
> 
>     shpool->data = ctx->value;
> 
>     return NGX\_OK;
> }

#### Logging

For logging nginx uses `ngx_log_t` objects. The nginx logger supports several types of output:

+   stderr — Logging to standard error (stderr)
+   file — Logging to a file
+   syslog — Logging to syslog
+   memory — Logging to internal memory storage for development purposes; the memory can be accessed later with a debugger

A logger instance can be a chain of loggers, linked to each other with the `next` field. In this case, each message is written to all loggers in the chain.

For each logger, a severity level controls which messages are written to the log (only events assigned that level or higher are logged). The following severity levels are supported:

+   `NGX_LOG_EMERG`
+   `NGX_LOG_ALERT`
+   `NGX_LOG_CRIT`
+   `NGX_LOG_ERR`
+   `NGX_LOG_WARN`
+   `NGX_LOG_NOTICE`
+   `NGX_LOG_INFO`
+   `NGX_LOG_DEBUG`

For debug logging, the debug mask is checked as well. The debug masks are:

+   `NGX_LOG_DEBUG_CORE`
+   `NGX_LOG_DEBUG_ALLOC`
+   `NGX_LOG_DEBUG_MUTEX`
+   `NGX_LOG_DEBUG_EVENT`
+   `NGX_LOG_DEBUG_HTTP`
+   `NGX_LOG_DEBUG_MAIL`
+   `NGX_LOG_DEBUG_STREAM`

Normally, loggers are created by existing nginx code from `error_log` directives and are available at nearly every stage of processing in cycle, configuration, client connection and other objects.

Nginx provides the following logging macros:

+   `ngx_log_error(level, log, err, fmt, ...)` — Error logging
+   `ngx_log_debug0(level, log, err, fmt)`, `ngx_log_debug1(level, log, err, fmt, arg1)` etc — Debug logging with up to eight supported formatting arguments

A log message is formatted in a buffer of size `NGX_MAX_ERROR_STR` (currently, 2048 bytes) on stack. The message is prepended with the severity level, process ID (PID), connection ID (stored in `log->connection`), and the system error text. For non-debug messages `log->handler` is called as well to prepend more specific information to the log message. HTTP module sets `ngx_http_log_error()` function as log handler to log client and server addresses, current action (stored in `log->action`), client request line, server name etc.

> /\* specify what is currently done \*/
> log->action = "sending mp4 to client";
> 
> /\* error and debug log \*/
> ngx\_log\_error(NGX\_LOG\_INFO, c->log, 0, "client prematurely
>               closed connection");
> 
> ngx\_log\_debug2(NGX\_LOG\_DEBUG\_HTTP, mp4->file.log, 0,
>                "mp4 start:%ui, length:%ui", mp4->start, mp4->length);

The example above results in log entries like these:

> 2016/09/16 22:08:52 \[info\] 17445#0: \*1 client prematurely closed connection while
> sending mp4 to client, client: 127.0.0.1, server: , request: "GET /file.mp4 HTTP/1.1"
> 2016/09/16 23:28:33 \[debug\] 22140#0: \*1 mp4 start:0, length:10000

#### Cycle

A cycle object stores the nginx runtime context created from a specific configuration. Its type is `ngx_cycle_t`. The current cycle is referenced by the `ngx_cycle` global variable and inherited by nginx workers as they start. Each time the nginx configuration is reloaded, a new cycle is created from the new nginx configuration; the old cycle is usually deleted after the new one is successfully created.

A cycle is created by the `ngx_init_cycle()` function, which takes the previous cycle as its argument. The function locates the previous cycle's configuration file and inherits as many resources as possible from the previous cycle. A placeholder cycle called "init cycle" is created as nginx start, then is replaced by an actual cycle built from configuration.

Members of the cycle include:

+   `pool` — Cycle pool. Created for each new cycle.
+   `log` — Cycle log. Initially inherited from the old cycle, it is set to point to `new_log` after the configuration is read.
+   `new_log` — Cycle log, created by the configuration. It's affected by the root-scope `error_log` directive.
+   `connections`, `connection_n` — Array of connections of type `ngx_connection_t`, created by the event module while initializing each nginx worker. The `worker_connections` directive in the nginx configuration sets the number of connections `connection_n`.
+   `free_connections`, `free_connection_n` — List and number of currently available connections. If no connections are available, an nginx worker refuses to accept new clients or connect to upstream servers.
+   `files`, `files_n` — Array for mapping file descriptors to nginx connections. This mapping is used by the event modules, having the `NGX_USE_FD_EVENT` flag (currently, it's `poll` and `devpoll`).
+   `conf_ctx` — Array of core module configurations. The configurations are created and filled during reading of nginx configuration files.
+   `modules`, `modules_n` — Array of modules of type `ngx_module_t`, both static and dynamic, loaded by the current configuration.
+   `listening` — Array of listening objects of type `ngx_listening_t`. Listening objects are normally added by the `listen` directive of different modules which call the `ngx_create_listening()` function. Listen sockets are created based on the listening objects.
+   `paths` — Array of paths of type `ngx_path_t`. Paths are added by calling the function `ngx_add_path()` from modules which are going to operate on certain directories. These directories are created by nginx after reading configuration, if missing. Moreover, two handlers can be added for each path:
    +   path loader — Executes only once in 60 seconds after starting or reloading nginx. Normally, the loader reads the directory and stores data in nginx shared memory. The handler is called from the dedicated nginx process “nginx cache loader”.
    +   path manager — Executes periodically. Normally, the manager removes old files from the directory and updates nginx memory to reflect the changes. The handler is called from the dedicated “nginx cache manager” process.
+   `open_files` — List of open file objects of type `ngx_open_file_t`, which are created by calling the function `ngx_conf_open_file()`. Currently, nginx uses this kind of open files for logging. After reading the configuration, nginx opens all files in the `open_files` list and stores each file descriptor in the object's `fd` field. The files are opened in append mode and are created if missing. The files in the list are reopened by nginx workers upon receiving the reopen signal (most often `USR1`). In this case the descriptor in the `fd` field is changed to a new value.
+   `shared_memory` — List of shared memory zones, each added by calling the `ngx_shared_memory_add()` function. Shared zones are mapped to the same address range in all nginx processes and are used to share common data, for example the HTTP cache in-memory tree.

#### Buffer

For input/output operations, nginx provides the buffer type `ngx_buf_t`. Normally, it's used to hold data to be written to a destination or read from a source. A buffer can reference data in memory or in a file and it's technically possible for a buffer to reference both at the same time. Memory for the buffer is allocated separately and is not related to the buffer structure `ngx_buf_t`.

The `ngx_buf_t` structure has the following fields:

+   `start`, `end` — The boundaries of the memory block allocated for the buffer.
+   `pos`, `last` — The boundaries of the memory buffer; normally a subrange of `start` .. `end`.
+   `file_pos`, `file_last` — The boundaries of a file buffer, expressed as offsets from the beginning of the file.
+   `tag` — Unique value used to distinguish buffers; created by different nginx modules, usually for the purpose of buffer reuse.
+   `file` — File object.
+   `temporary` — Flag indicating that the buffer references writable memory.
+   `memory` — Flag indicating that the buffer references read-only memory.
+   `in_file` — Flag indicating that the buffer references data in a file.
+   `flush` — Flag indicating that all data prior to the buffer need to be flushed.
+   `recycled` — Flag indicating that the buffer can be reused and needs to be consumed as soon as possible.
+   `sync` — Flag indicating that the buffer carries no data or special signal like `flush` or `last_buf`. By default nginx considers such buffers an error condition, but this flag tells nginx to skip the error check.
+   `last_buf` — Flag indicating that the buffer is the last in output.
+   `last_in_chain` — Flag indicating that there are no more data buffers in a request or subrequest.
+   `shadow` — Reference to another ("shadow") buffer related to the current buffer, usually in the sense that the buffer uses data from the shadow. When the buffer is consumed, the shadow buffer is normally also marked as consumed.
+   `last_shadow` — Flag indicating that the buffer is the last one that references a particular shadow buffer.
+   `temp_file` — Flag indicating that the buffer is in a temporary file.

For input and output operations buffers are linked in chains. A chain is a sequence of chain links of type `ngx_chain_t`, defined as follows:

> typedef struct ngx\_chain\_s  ngx\_chain\_t;
> 
> struct ngx\_chain\_s {
>     ngx\_buf\_t    \*buf;
>     ngx\_chain\_t  \*next;
> };

Each chain link keeps a reference to its buffer and a reference to the next chain link.

An example of using buffers and chains:

> ngx\_chain\_t \*
> ngx\_get\_my\_chain(ngx\_pool\_t \*pool)
> {
>     ngx\_buf\_t    \*b;
>     ngx\_chain\_t  \*out, \*cl, \*\*ll;
> 
>     /\* first buf \*/
>     cl = ngx\_alloc\_chain\_link(pool);
>     if (cl == NULL) { /\* error \*/ }
> 
>     b = ngx\_calloc\_buf(pool);
>     if (b == NULL) { /\* error \*/ }
> 
>     b->start = (u\_char \*) "foo";
>     b->pos = b->start;
>     b->end = b->start + 3;
>     b->last = b->end;
>     b->memory = 1; /\* read-only memory \*/
> 
>     cl->buf = b;
>     out = cl;
>     ll = &cl->next;
> 
>     /\* second buf \*/
>     cl = ngx\_alloc\_chain\_link(pool);
>     if (cl == NULL) { /\* error \*/ }
> 
>     b = ngx\_create\_temp\_buf(pool, 3);
>     if (b == NULL) { /\* error \*/ }
> 
>     b->last = ngx\_cpymem(b->last, "foo", 3);
> 
>     cl->buf = b;
>     cl->next = NULL;
>     \*ll = cl;
> 
>     return out;
> }

#### Networking

#### Connection

The connection type `ngx_connection_t` is a wrapper around a socket descriptor. It includes the following fields:

+   `fd` — Socket descriptor
+   `data` — Arbitrary connection context. Normally, it is a pointer to a higher-level object built on top of the connection, such as an HTTP request or a Stream session.
+   `read`, `write` — Read and write events for the connection.
+   `recv`, `send`, `recv_chain`, `send_chain` — I/O operations for the connection.
+   `pool` — Connection pool.
+   `log` — Connection log.
+   `sockaddr`, `socklen`, `addr_text` — Remote socket address in binary and text forms.
+   `local_sockaddr`, `local_socklen` — Local socket address in binary form. Initially, these fields are empty. Use the `ngx_connection_local_sockaddr()` function to get the local socket address.
+   `proxy_protocol_addr`, `proxy_protocol_port` - PROXY protocol client address and port, if the PROXY protocol is enabled for the connection.
+   `ssl` — SSL context for the connection.
+   `reusable` — Flag indicating the connection is in a state that makes it eligible for reuse.
+   `close` — Flag indicating that the connection is being reused and needs to be closed.

An nginx connection can transparently encapsulate the SSL layer. In this case the connection's `ssl` field holds a pointer to an `ngx_ssl_connection_t` structure, keeping all SSL-related data for the connection, including `SSL_CTX` and `SSL`. The `recv`, `send`, `recv_chain`, and `send_chain` handlers are set to SSL-enabled functions as well.

The `worker_connections` directive in the nginx configuration limits the number of connections per nginx worker. All connection structures are precreated when a worker starts and stored in the `connections` field of the cycle object. To retrieve a connection structure, use the `ngx_get_connection(s, log)` function. It takes as its `s` argument a socket descriptor, which needs to be wrapped in a connection structure.

Because the number of connections per worker is limited, nginx provides a way to grab connections that are currently in use. To enable or disable reuse of a connection, call the `ngx_reusable_connection(c, reusable)` function. Calling `ngx_reusable_connection(c, 1)` sets the `reuse` flag in the connection structure and inserts the connection into the `reusable_connections_queue` of the cycle. Whenever `ngx_get_connection()` finds out there are no available connections in the cycle's `free_connections` list, it calls `ngx_drain_connections()` to release a specific number of reusable connections. For each such connection, the `close` flag is set and its read handler is called which is supposed to free the connection by calling `ngx_close_connection(c)` and make it available for reuse. To exit the state when a connection can be reused `ngx_reusable_connection(c, 0)` is called. HTTP client connections are an example of reusable connections in nginx; they are marked as reusable until the first request byte is received from the client.

#### Events

#### Event

Event object `ngx_event_t` in nginx provides a mechanism for notification that a specific event has occurred.

Fields in `ngx_event_t` include:

+   `data` — Arbitrary event context used in event handlers, usually as pointer to a connection related to the event.
+   `handler` — Callback function to be invoked when the event happens.
+   `write` — Flag indicating a write event. Absence of the flag indicates a read event.
+   `active` — Flag indicating that the event is registered for receiving I/O notifications, normally from notification mechanisms like `epoll`, `kqueue`, `poll`.
+   `ready` — Flag indicating that the event has received an I/O notification.
+   `delayed` — Flag indicating that I/O is delayed due to rate limiting.
+   `timer` — Red-black tree node for inserting the event into the timer tree.
+   `timer_set` — Flag indicating that the event timer is set and not yet expired.
+   `timedout` — Flag indicating that the event timer has expired.
+   `eof` — Flag indicating that EOF occurred while reading data.
+   `pending_eof` — Flag indicating that EOF is pending on the socket, even though there may be some data available before it. The flag is delivered via the `EPOLLRDHUP` `epoll` event or `EV_EOF` `kqueue` flag.
+   `error` — Flag indicating that an error occurred during reading (for a read event) or writing (for a write event).
+   `cancelable` — Timer event flag indicating that the event should be ignored while shutting down the worker. Graceful worker shutdown is delayed until there are no non-cancelable timer events scheduled.
+   `posted` — Flag indicating that the event is posted to a queue.
+   `queue` — Queue node for posting the event to a queue.

#### I/O events

Each connection obtained by calling the `ngx_get_connection()` function has two attached events, `c->read` and `c->write`, which are used for receiving notification that the socket is ready for reading or writing. All such events operate in Edge-Triggered mode, meaning that they only trigger notifications when the state of the socket changes. For example, doing a partial read on a socket does not make nginx deliver a repeated read notification until more data arrives on the socket. Even when the underlying I/O notification mechanism is essentially Level-Triggered (`poll`, `select` etc), nginx converts the notifications to Edge-Triggered. To make nginx event notifications consistent across all notifications systems on different platforms, the functions `ngx_handle_read_event(rev, flags)` and `ngx_handle_write_event(wev, lowat)` must be called after handling an I/O socket notification or calling any I/O functions on that socket. Normally, the functions are called once at the end of each read or write event handler.

#### Timer events

An event can be set to send a notification when a timeout expires. The timer used by events counts milliseconds since some unspecified point in the past truncated to `ngx_msec_t` type. Its current value can be obtained from the `ngx_current_msec` variable.

The function `ngx_add_timer(ev, timer)` sets a timeout for an event, `ngx_del_timer(ev)` deletes a previously set timeout. The global timeout red-black tree `ngx_event_timer_rbtree` stores all timeouts currently set. The key in the tree is of type `ngx_msec_t` and is the time when the event occurs. The tree structure enables fast insertion and deletion operations, as well as access to the nearest timeouts, which nginx uses to find out how long to wait for I/O events and for expiring timeout events.

#### Posted events

An event can be posted which means that its handler will be called at some point later within the current event loop iteration. Posting events is a good practice for simplifying code and escaping stack overflows. Posted events are held in a post queue. The `ngx_post_event(ev, q)` macro posts the event `ev` to the post queue `q`. The `ngx_delete_posted_event(ev)` macro deletes the event `ev` from the queue it's currently posted in. Normally, events are posted to the `ngx_posted_events` queue, which is processed late in the event loop — after all I/O and timer events are already handled. The function `ngx_event_process_posted()` is called to process an event queue. It calls event handlers until the queue is not empty. This means that a posted event handler can post more events to be processed within the current event loop iteration.

An example:

> void
> ngx\_my\_connection\_read(ngx\_connection\_t \*c)
> {
>     ngx\_event\_t  \*rev;
> 
>     rev = c->read;
> 
>     ngx\_add\_timer(rev, 1000);
> 
>     rev->handler = ngx\_my\_read\_handler;
> 
>     ngx\_my\_read(rev);
> }
> 
> 
> void
> ngx\_my\_read\_handler(ngx\_event\_t \*rev)
> {
>     ssize\_t            n;
>     ngx\_connection\_t  \*c;
>     u\_char             buf\[256\];
> 
>     if (rev->timedout) { /\* timeout expired \*/ }
> 
>     c = rev->data;
> 
>     while (rev->ready) {
>         n = c->recv(c, buf, sizeof(buf));
> 
>         if (n == NGX\_AGAIN) {
>             break;
>         }
> 
>         if (n == NGX\_ERROR) { /\* error \*/ }
> 
>         /\* process buf \*/
>     }
> 
>     if (ngx\_handle\_read\_event(rev, 0) != NGX\_OK) { /\* error \*/ }
> }

#### Event loop

Except for the nginx master process, all nginx processes do I/O and so have an event loop. (The nginx master process instead spends most of its time in the `sigsuspend()` call waiting for signals to arrive.) The nginx event loop is implemented in the `ngx_process_events_and_timers()` function, which is called repeatedly until the process exits.

The event loop has the following stages:

+   Find the timeout that is closest to expiring, by calling `ngx_event_find_timer()`. This function finds the leftmost node in the timer tree and returns the number of milliseconds until the node expires.
+   Process I/O events by calling a handler, specific to the event notification mechanism, chosen by nginx configuration. This handler waits for at least one I/O event to happen, but only until the next timeout expires. When a read or write event occurs, the `ready` flag is set and the event's handler is called. For Linux, the `ngx_epoll_process_events()` handler is normally used, which calls `epoll_wait()` to wait for I/O events.
+   Expire timers by calling `ngx_event_expire_timers()`. The timer tree is iterated from the leftmost element to the right until an unexpired timeout is found. For each expired node the `timedout` event flag is set, the `timer_set` flag is reset, and the event handler is called
+   Process posted events by calling `ngx_event_process_posted()`. The function repeatedly removes the first element from the posted events queue and calls the element's handler, until the queue is empty.

All nginx processes handle signals as well. Signal handlers only set global variables which are checked after the `ngx_process_events_and_timers()` call.

#### Processes

There are several types of processes in nginx. The type of a process is kept in the `ngx_process` global variable, and is one of the following:

+   `NGX_PROCESS_MASTER` — The master process, which reads the NGINX configuration, creates cycles, and starts and controls child processes. It does not perform any I/O and responds only to signals. Its cycle function is `ngx_master_process_cycle()`.
    
+   `NGX_PROCESS_WORKER` — The worker process, which handles client connections. It is started by the master process and responds to its signals and channel commands as well. Its cycle function is `ngx_worker_process_cycle()`. There can be multiple worker processes, as configured by the `worker_processes` directive.
    
+   `NGX_PROCESS_SINGLE` — The single process, which exists only in `master_process off` mode, and is the only process running in that mode. It creates cycles (like the master process does) and handles client connections (like the worker process does). Its cycle function is `ngx_single_process_cycle()`.
    
+   `NGX_PROCESS_HELPER` — The helper process, of which currently there are two types: cache manager and cache loader. The cycle function for both is `ngx_cache_manager_process_cycle()`.
    

The nginx processes handle the following signals:

+   `NGX_SHUTDOWN_SIGNAL` (`SIGQUIT` on most systems) — Gracefully shutdown. Upon receiving this signal, the master process sends a shutdown signal to all child processes. When no child processes are left, the master destroys the cycle pool and exits. When a worker process receives this signal, it closes all listening sockets and waits until there are no non-cancelable events scheduled, then destroys the cycle pool and exits. When the cache manager or the cache loader process receives this signal, it exits immediately. The `ngx_quit` variable is set to `1` when a process receives this signal, and is immediately reset after being processed. The `ngx_exiting` variable is set to `1` while a worker process is in the shutdown state.
    
+   `NGX_TERMINATE_SIGNAL` (`SIGTERM` on most systems) — Terminate. Upon receiving this signal, the master process sends a terminate signal to all child processes. If a child process does not exit within 1 second, the master process sends the `SIGKILL` signal to kill it. When no child processes are left, the master process destroys the cycle pool and exits. When a worker process, the cache manager process or the cache loader process receives this signal, it destroys the cycle pool and exits. The variable `ngx_terminate` is set to `1` when this signal is received.
    
+   `NGX_NOACCEPT_SIGNAL` (`SIGWINCH` on most systems) - Shut down all worker and helper processes. Upon receiving this signal, the master process shuts down its child processes. If a previously started new nginx binary exits, the child processes of the old master are started again. When a worker process receives this signal, it shuts down in debug mode set by the `debug_points` directive.
    
+   `NGX_RECONFIGURE_SIGNAL` (`SIGHUP` on most systems) - Reconfigure. Upon receiving this signal, the master process re-reads the configuration and creates a new cycle based on it. If the new cycle is created successfully, the old cycle is deleted and new child processes are started. Meanwhile, the old child processes receive the `NGX_SHUTDOWN_SIGNAL` signal. In single-process mode, nginx creates a new cycle, but keeps the old one until there are no longer clients with active connections tied to it. The worker and helper processes ignore this signal.
    
+   `NGX_REOPEN_SIGNAL` (`SIGUSR1` on most systems) — Reopen files. The master process sends this signal to workers, which reopen all `open_files` related to the cycle.
    
+   `NGX_CHANGEBIN_SIGNAL` (`SIGUSR2` on most systems) — Change the nginx binary. The master process starts a new nginx binary and passes in a list of all listen sockets. The text-format list, passed in the `“NGINX”` environment variable, consists of descriptor numbers separated with semicolons. The new nginx binary reads the `“NGINX”` variable and adds the sockets to its init cycle. Other processes ignore this signal.
    

While all nginx worker processes are able to receive and properly handle POSIX signals, the master process does not use the standard `kill()` syscall to pass signals to workers and helpers. Instead, nginx uses inter-process socket pairs which allow sending messages between all nginx processes. Currently, however, messages are only sent from the master to its children. The messages carry the standard signals.

#### Threads

It is possible to offload into a separate thread tasks that would otherwise block the nginx worker process. For example, nginx can be configured to use threads to perform [file I/O](../http/ngx_http_core_module.html#aio). Another use case is a library that doesn't have asynchronous interface and thus cannot be normally used with nginx. Keep in mind that the threads interface is a helper for the existing asynchronous approach to processing client connections, and by no means intended as a replacement.

To deal with synchronization, the following wrappers over `pthreads` primitives are available:

+   `typedef pthread_mutex_t ngx_thread_mutex_t;`
    +   `ngx_int_t ngx_thread_mutex_create(ngx_thread_mutex_t *mtx, ngx_log_t *log);`
    +   `ngx_int_t ngx_thread_mutex_destroy(ngx_thread_mutex_t *mtx, ngx_log_t *log);`
    +   `ngx_int_t ngx_thread_mutex_lock(ngx_thread_mutex_t *mtx, ngx_log_t *log);`
    +   `ngx_int_t ngx_thread_mutex_unlock(ngx_thread_mutex_t *mtx, ngx_log_t *log);`
+   `typedef pthread_cond_t ngx_thread_cond_t;`
    +   `ngx_int_t ngx_thread_cond_create(ngx_thread_cond_t *cond, ngx_log_t *log);`
    +   `ngx_int_t ngx_thread_cond_destroy(ngx_thread_cond_t *cond, ngx_log_t *log);`
    +   `ngx_int_t ngx_thread_cond_signal(ngx_thread_cond_t *cond, ngx_log_t *log);`
    +   `ngx_int_t ngx_thread_cond_wait(ngx_thread_cond_t *cond, ngx_thread_mutex_t *mtx, ngx_log_t *log);`

Instead of creating a new thread for each task, nginx implements a [thread\_pool](../ngx_core_module.html#thread_pool) strategy. Multiple thread pools may be configured for different purposes (for example, performing I/O on different sets of disks). Each thread pool is created at startup and contains a limited number of threads that process a queue of tasks. When a task is completed, a predefined completion handler is called.

The `src/core/ngx_thread_pool.h` header file contains relevant definitions:

> struct ngx\_thread\_task\_s {
>     ngx\_thread\_task\_t   \*next;
>     ngx\_uint\_t           id;
>     void                \*ctx;
>     void               (\*handler)(void \*data, ngx\_log\_t \*log);
>     ngx\_event\_t          event;
> };
> 
> typedef struct ngx\_thread\_pool\_s  ngx\_thread\_pool\_t;
> 
> ngx\_thread\_pool\_t \*ngx\_thread\_pool\_add(ngx\_conf\_t \*cf, ngx\_str\_t \*name);
> ngx\_thread\_pool\_t \*ngx\_thread\_pool\_get(ngx\_cycle\_t \*cycle, ngx\_str\_t \*name);
> 
> ngx\_thread\_task\_t \*ngx\_thread\_task\_alloc(ngx\_pool\_t \*pool, size\_t size);
> ngx\_int\_t ngx\_thread\_task\_post(ngx\_thread\_pool\_t \*tp, ngx\_thread\_task\_t \*task);

At configuration time, a module willing to use threads has to obtain a reference to a thread pool by calling `ngx_thread_pool_add(cf, name)`, which either creates a new thread pool with the given `name` or returns a reference to the pool with that name if it already exists.

To add a `task` into a queue of a specified thread pool `tp` at runtime, use the `ngx_thread_task_post(tp, task)` function. To execute a function in a thread, pass parameters and setup a completion handler using the `ngx_thread_task_t` structure:

> typedef struct {
>     int    foo;
> } my\_thread\_ctx\_t;
> 
> 
> static void
> my\_thread\_func(void \*data, ngx\_log\_t \*log)
> {
>     my\_thread\_ctx\_t \*ctx = data;
> 
>     /\* this function is executed in a separate thread \*/
> }
> 
> 
> static void
> my\_thread\_completion(ngx\_event\_t \*ev)
> {
>     my\_thread\_ctx\_t \*ctx = ev->data;
> 
>     /\* executed in nginx event loop \*/
> }
> 
> 
> ngx\_int\_t
> my\_task\_offload(my\_conf\_t \*conf)
> {
>     my\_thread\_ctx\_t    \*ctx;
>     ngx\_thread\_task\_t  \*task;
> 
>     task = ngx\_thread\_task\_alloc(conf->pool, sizeof(my\_thread\_ctx\_t));
>     if (task == NULL) {
>         return NGX\_ERROR;
>     }
> 
>     ctx = task->ctx;
> 
>     ctx->foo = 42;
> 
>     task->handler = my\_thread\_func;
>     task->event.handler = my\_thread\_completion;
>     task->event.data = ctx;
> 
>     if (ngx\_thread\_task\_post(conf->thread\_pool, task) != NGX\_OK) {
>         return NGX\_ERROR;
>     }
> 
>     return NGX\_OK;
> }

#### Modules

#### Adding new modules

Each standalone nginx module resides in a separate directory that contains at least two files: `config` and a file with the module source code. The `config` file contains all information needed for nginx to integrate the module, for example:

> ngx\_module\_type=CORE
> ngx\_module\_name=ngx\_foo\_module
> ngx\_module\_srcs="$ngx\_addon\_dir/ngx\_foo\_module.c"
> 
> . auto/module
> 
> ngx\_addon\_name=$ngx\_module\_name

The `config` file is a POSIX shell script that can set and access the following variables:

+   `ngx_module_type` — Type of module to build. Possible values are `CORE`, `HTTP`, `HTTP_FILTER`, `HTTP_INIT_FILTER`, `HTTP_AUX_FILTER`, `MAIL`, `STREAM`, or `MISC`.
+   `ngx_module_name` — Module names. To build multiple modules from a set of source files, specify a whitespace-separated list of names. The first name indicates the name of the output binary for the dynamic module. The names in the list must match the names used in the source code.
+   `ngx_addon_name` — Name of the module as it appears in output on the console from the configure script.
+   `ngx_module_srcs` — Whitespace-separated list of source files used to compile the module. The `$ngx_addon_dir` variable can be used to represent the path to the module directory.
+   `ngx_module_incs` — Include paths required to build the module
+   `ngx_module_deps` — Whitespace-separated list of the module's dependencies. Usually, it is the list of header files.
+   `ngx_module_libs` — Whitespace-separated list of libraries to link with the module. For example, use `ngx_module_libs=-lpthread` to link `libpthread` library. The following macros can be used to link against the same libraries as nginx: `LIBXSLT`, `LIBGD`, `GEOIP`, `PCRE`, `OPENSSL`, `MD5`, `SHA1`, `ZLIB`, and `PERL`.
+   `ngx_module_link` — Variable set by the build system to `DYNAMIC` for a dynamic module or `ADDON` for a static module and used to determine different actions to perform depending on linking type.
+   `ngx_module_order` — Load order for the module; useful for the `HTTP_FILTER` and `HTTP_AUX_FILTER` module types. The format for this option is a whitespace-separated list of modules. All modules in the list following the current module's name end up after it in the global list of modules, which sets up the order for modules initialization. For filter modules later initialization means earlier execution.
    
    The following modules are typically used as references. The `ngx_http_copy_filter_module` reads the data for other filter modules and is placed near the bottom of the list so that it is one of the first to be executed. The `ngx_http_write_filter_module` writes the data to the client socket and is placed near the top of the list, and is the last to be executed.
    
    By default, filter modules are placed before the `ngx_http_copy_filter` in the module list so that the filter handler is executed after the copy filter handler. For other module types the default is the empty string.
    

To compile a module into nginx statically, use the `--add-module=/path/to/module` argument to the configure script. To compile a module for later dynamic loading into nginx, use the `--add-dynamic-module=/path/to/module` argument.

#### Core Modules

Modules are the building blocks of nginx, and most of its functionality is implemented as modules. The module source file must contain a global variable of type `ngx_module_t`, which is defined as follows:

> struct ngx\_module\_s {
> 
>     /\* private part is omitted \*/
> 
>     void                 \*ctx;
>     ngx\_command\_t        \*commands;
>     ngx\_uint\_t            type;
> 
>     ngx\_int\_t           (\*init\_master)(ngx\_log\_t \*log);
> 
>     ngx\_int\_t           (\*init\_module)(ngx\_cycle\_t \*cycle);
> 
>     ngx\_int\_t           (\*init\_process)(ngx\_cycle\_t \*cycle);
>     ngx\_int\_t           (\*init\_thread)(ngx\_cycle\_t \*cycle);
>     void                (\*exit\_thread)(ngx\_cycle\_t \*cycle);
>     void                (\*exit\_process)(ngx\_cycle\_t \*cycle);
> 
>     void                (\*exit\_master)(ngx\_cycle\_t \*cycle);
> 
>     /\* stubs for future extensions are omitted \*/
> };

The omitted private part includes the module version and a signature and is filled using the predefined macro `NGX_MODULE_V1`.

Each module keeps its private data in the `ctx` field, recognizes the configuration directives, specified in the `commands` array, and can be invoked at certain stages of nginx lifecycle. The module lifecycle consists of the following events:

+   Configuration directive handlers are called as they appear in configuration files in the context of the master process.
+   After the configuration is parsed successfully, `init_module` handler is called in the context of the master process. The `init_module` handler is called in the master process each time a configuration is loaded.
+   The master process creates one or more worker processes and the `init_process` handler is called in each of them.
+   When a worker process receives the shutdown or terminate command from the master, it invokes the `exit_process` handler.
+   The master process calls the `exit_master` handler before exiting.

Because threads are used in nginx only as a supplementary I/O facility with its own API, `init_thread` and `exit_thread` handlers are not currently called. There is also no `init_master` handler, because it would be unnecessary overhead.

The module `type` defines exactly what is stored in the `ctx` field. Its value is one of the following types:

+   `NGX_CORE_MODULE`
+   `NGX_EVENT_MODULE`
+   `NGX_HTTP_MODULE`
+   `NGX_MAIL_MODULE`
+   `NGX_STREAM_MODULE`

The `NGX_CORE_MODULE` is the most basic and thus the most generic and most low-level type of module. The other module types are implemented on top of it and provide a more convenient way to deal with corresponding domains, like handling events or HTTP requests.

The set of core modules includes `ngx_core_module`, `ngx_errlog_module`, `ngx_regex_module`, `ngx_thread_pool_module` and `ngx_openssl_module` modules. The HTTP module, the stream module, the mail module and event modules are core modules too. The context of a core module is defined as:

> typedef struct {
>     ngx\_str\_t             name;
>     void               \*(\*create\_conf)(ngx\_cycle\_t \*cycle);
>     char               \*(\*init\_conf)(ngx\_cycle\_t \*cycle, void \*conf);
> } ngx\_core\_module\_t;

where the `name` is a module name string, `create_conf` and `init_conf` are pointers to functions that create and initialize module configuration respectively. For core modules, nginx calls `create_conf` before parsing a new configuration and `init_conf` after all configuration is parsed successfully. The typical `create_conf` function allocates memory for the configuration and sets default values.

For example, a simplistic module called `ngx_foo_module` might look like this:

> /\*
>  \* Copyright (C) Author.
>  \*/
> 
> 
> #include <ngx\_config.h>
> #include <ngx\_core.h>
> 
> 
> typedef struct {
>     ngx\_flag\_t  enable;
> } ngx\_foo\_conf\_t;
> 
> 
> static void \*ngx\_foo\_create\_conf(ngx\_cycle\_t \*cycle);
> static char \*ngx\_foo\_init\_conf(ngx\_cycle\_t \*cycle, void \*conf);
> 
> static char \*ngx\_foo\_enable(ngx\_conf\_t \*cf, void \*post, void \*data);
> static ngx\_conf\_post\_t  ngx\_foo\_enable\_post = { ngx\_foo\_enable };
> 
> 
> static ngx\_command\_t  ngx\_foo\_commands\[\] = {
> 
>     { ngx\_string("foo\_enabled"),
>       NGX\_MAIN\_CONF|NGX\_DIRECT\_CONF|NGX\_CONF\_FLAG,
>       ngx\_conf\_set\_flag\_slot,
>       0,
>       offsetof(ngx\_foo\_conf\_t, enable),
>       &ngx\_foo\_enable\_post },
> 
>       ngx\_null\_command
> };
> 
> 
> static ngx\_core\_module\_t  ngx\_foo\_module\_ctx = {
>     ngx\_string("foo"),
>     ngx\_foo\_create\_conf,
>     ngx\_foo\_init\_conf
> };
> 
> 
> ngx\_module\_t  ngx\_foo\_module = {
>     NGX\_MODULE\_V1,
>     &ngx\_foo\_module\_ctx,                   /\* module context \*/
>     ngx\_foo\_commands,                      /\* module directives \*/
>     NGX\_CORE\_MODULE,                       /\* module type \*/
>     NULL,                                  /\* init master \*/
>     NULL,                                  /\* init module \*/
>     NULL,                                  /\* init process \*/
>     NULL,                                  /\* init thread \*/
>     NULL,                                  /\* exit thread \*/
>     NULL,                                  /\* exit process \*/
>     NULL,                                  /\* exit master \*/
>     NGX\_MODULE\_V1\_PADDING
> };
> 
> 
> static void \*
> ngx\_foo\_create\_conf(ngx\_cycle\_t \*cycle)
> {
>     ngx\_foo\_conf\_t  \*fcf;
> 
>     fcf = ngx\_pcalloc(cycle->pool, sizeof(ngx\_foo\_conf\_t));
>     if (fcf == NULL) {
>         return NULL;
>     }
> 
>     fcf->enable = NGX\_CONF\_UNSET;
> 
>     return fcf;
> }
> 
> 
> static char \*
> ngx\_foo\_init\_conf(ngx\_cycle\_t \*cycle, void \*conf)
> {
>     ngx\_foo\_conf\_t \*fcf = conf;
> 
>     ngx\_conf\_init\_value(fcf->enable, 0);
> 
>     return NGX\_CONF\_OK;
> }
> 
> 
> static char \*
> ngx\_foo\_enable(ngx\_conf\_t \*cf, void \*post, void \*data)
> {
>     ngx\_flag\_t  \*fp = data;
> 
>     if (\*fp == 0) {
>         return NGX\_CONF\_OK;
>     }
> 
>     ngx\_log\_error(NGX\_LOG\_NOTICE, cf->log, 0, "Foo Module is enabled");
> 
>     return NGX\_CONF\_OK;
> }

#### Configuration Directives

The `ngx_command_t` type defines a single configuration directive. Each module that supports configuration provides an array of such structures that describe how to process arguments and what handlers to call:

> typedef struct ngx\_command\_s  ngx\_command\_t;
> 
> struct ngx\_command\_s {
>     ngx\_str\_t             name;
>     ngx\_uint\_t            type;
>     char               \*(\*set)(ngx\_conf\_t \*cf, ngx\_command\_t \*cmd, void \*conf);
>     ngx\_uint\_t            conf;
>     ngx\_uint\_t            offset;
>     void                 \*post;
> };

Terminate the array with the special value `ngx_null_command`. The `name` is the name of a directive as it appears in the configuration file, for example "worker\_processes" or "listen". The `type` is a bit-field of flags that specify the number of arguments the directive takes, its type, and the context in which it appears. The flags are:

+   `NGX_CONF_NOARGS` — Directive takes no arguments.
+   `NGX_CONF_1MORE` — Directive takes one or more arguments.
+   `NGX_CONF_2MORE` — Directive takes two or more arguments.
+   `NGX_CONF_TAKE1`..`NGX_CONF_TAKE7` — Directive takes exactly the indicated number of arguments.
+   `NGX_CONF_TAKE12`, `NGX_CONF_TAKE13`, `NGX_CONF_TAKE23`, `NGX_CONF_TAKE123`, `NGX_CONF_TAKE1234` — Directive may take different number of arguments. Options are limited to the given numbers. For example, `NGX_CONF_TAKE12` means it takes one or two arguments.

The flags for directive types are:

+   `NGX_CONF_BLOCK` — Directive is a block, that is, it can contain other directives within its opening and closing braces, or even implement its own parser to handle contents inside.
+   `NGX_CONF_FLAG` — Directive takes a boolean value, either `on` or `off`.

A directive's context defines where it may appear in the configuration:

+   `NGX_MAIN_CONF` — In the top level context.
+   `NGX_HTTP_MAIN_CONF` — In the `http` block.
+   `NGX_HTTP_SRV_CONF` — In a `server` block within the `http` block.
+   `NGX_HTTP_LOC_CONF` — In a `location` block within the `http` block.
+   `NGX_HTTP_UPS_CONF` — In an `upstream` block within the `http` block.
+   `NGX_HTTP_SIF_CONF` — In an `if` block within a `server` block in the `http` block.
+   `NGX_HTTP_LIF_CONF` — In an `if` block within a `location` block in the `http` block.
+   `NGX_HTTP_LMT_CONF` — In a `limit_except` block within the `http` block.
+   `NGX_STREAM_MAIN_CONF` — In the `stream` block.
+   `NGX_STREAM_SRV_CONF` — In a `server` block within the `stream` block.
+   `NGX_STREAM_UPS_CONF` — In an `upstream` block within the `stream` block.
+   `NGX_MAIL_MAIN_CONF` — In the `mail` block.
+   `NGX_MAIL_SRV_CONF` — In a `server` block within the `mail` block.
+   `NGX_EVENT_CONF` — In the `event` block.
+   `NGX_DIRECT_CONF` — Used by modules that don't create a hierarchy of contexts and only have one global configuration. This configuration is passed to the handler as the `conf` argument.

The configuration parser uses these flags to throw an error in case of a misplaced directive and calls directive handlers supplied with a proper configuration pointer, so that the same directives in different locations can store their values in distinct places.

The `set` field defines a handler that processes a directive and stores parsed values into the corresponding configuration. There's a number of functions that perform common conversions:

+   `ngx_conf_set_flag_slot` — Converts the literal strings `on` and `off` into an `ngx_flag_t` value with values 1 or 0, respectively.
+   `ngx_conf_set_str_slot` — Stores a string as a value of the `ngx_str_t` type.
+   `ngx_conf_set_str_array_slot` — Appends a value to an array `ngx_array_t` of strings `ngx_str_t`. The array is created if does not already exist.
+   `ngx_conf_set_keyval_slot` — Appends a key-value pair to an array `ngx_array_t` of key-value pairs `ngx_keyval_t`. The first string becomes the key and the second the value. The array is created if it does not already exist.
+   `ngx_conf_set_num_slot` — Converts a directive's argument to an `ngx_int_t` value.
+   `ngx_conf_set_size_slot` — Converts a [size](../syntax.html) to a `size_t` value expressed in bytes.
+   `ngx_conf_set_off_slot` — Converts an [offset](../syntax.html) to an `off_t` value expressed in bytes.
+   `ngx_conf_set_msec_slot` — Converts a [time](../syntax.html) to an `ngx_msec_t` value expressed in milliseconds.
+   `ngx_conf_set_sec_slot` — Converts a [time](../syntax.html) to a `time_t` value expressed in in seconds.
+   `ngx_conf_set_bufs_slot` — Converts the two supplied arguments into an `ngx_bufs_t` object that holds the number and [size](../syntax.html) of buffers.
+   `ngx_conf_set_enum_slot` — Converts the supplied argument into an `ngx_uint_t` value. The null-terminated array of `ngx_conf_enum_t` passed in the `post` field defines the acceptable strings and corresponding integer values.
+   `ngx_conf_set_bitmask_slot` — Converts the supplied arguments into an `ngx_uint_t` value. The mask values for each argument are ORed producing the result. The null-terminated array of `ngx_conf_bitmask_t` passed in the `post` field defines the acceptable strings and corresponding mask values.
+   `set_path_slot` — Converts the supplied arguments to an `ngx_path_t` value and performs all required initializations. For details, see the documentation for the [proxy\_temp\_path](../http/ngx_http_proxy_module.html#proxy_temp_path) directive.
+   `set_access_slot` — Converts the supplied arguments to a file permissions mask. For details, see the documentation for the [proxy\_store\_access](../http/ngx_http_proxy_module.html#proxy_store_access) directive.

The `conf` field defines which configuration structure is passed to the directory handler. Core modules only have the global configuration and set `NGX_DIRECT_CONF` flag to access it. Modules like HTTP, Stream or Mail create hierarchies of configurations. For example, a module's configuration is created for `server`, `location` and `if` scopes.

+   `NGX_HTTP_MAIN_CONF_OFFSET` — Configuration for the `http` block.
+   `NGX_HTTP_SRV_CONF_OFFSET` — Configuration for a `server` block within the `http` block.
+   `NGX_HTTP_LOC_CONF_OFFSET` — Configuration for a `location` block within the `http`.
+   `NGX_STREAM_MAIN_CONF_OFFSET` — Configuration for the `stream` block.
+   `NGX_STREAM_SRV_CONF_OFFSET` — Configuration for a `server` block within the `stream` block.
+   `NGX_MAIL_MAIN_CONF_OFFSET` — Configuration for the `mail` block.
+   `NGX_MAIL_SRV_CONF_OFFSET` — Configuration for a `server` block within the `mail` block.

The `offset` defines the offset of a field in a module configuration structure that holds values for this particular directive. The typical use is to employ the `offsetof()` macro.

The `post` field has two purposes: it may be used to define a handler to be called after the main handler has completed, or to pass additional data to the main handler. In the first case, the `ngx_conf_post_t` structure needs to be initialized with a pointer to the handler, for example:

> static char \*ngx\_do\_foo(ngx\_conf\_t \*cf, void \*post, void \*data);
> static ngx\_conf\_post\_t  ngx\_foo\_post = { ngx\_do\_foo };

The `post` argument is the `ngx_conf_post_t` object itself, and the `data` is a pointer to the value, converted from arguments by the main handler with the appropriate type.

#### HTTP

#### Connection

Each HTTP client connection runs through the following stages:

+   `ngx_event_accept()` accepts a client TCP connection. This handler is called in response to a read notification on a listen socket. A new `ngx_connection_t` object is created at this stage to wrap the newly accepted client socket. Each nginx listener provides a handler to pass the new connection object to. For HTTP connections it's `ngx_http_init_connection(c)`.
+   `ngx_http_init_connection()` performs early initialization of the HTTP connection. At this stage an `ngx_http_connection_t` object is created for the connection and its reference is stored in the connection's `data` field. Later it will be replaced by an HTTP request object. A PROXY protocol parser and the SSL handshake are started at this stage as well.
+   `ngx_http_wait_request_handler()` read event handler is called when data is available on the client socket. At this stage an HTTP request object `ngx_http_request_t` is created and set to the connection's `data` field.
+   `ngx_http_process_request_line()` read event handler reads client request line. The handler is set by `ngx_http_wait_request_handler()`. The data is read into connection's `buffer`. The size of the buffer is initially set by the directive [client\_header\_buffer\_size](../http/ngx_http_core_module.html#client_header_buffer_size). The entire client header is supposed to fit in the buffer. If the initial size is not sufficient, a bigger buffer is allocated, with the capacity set by the `large_client_header_buffers` directive.
+   `ngx_http_process_request_headers()` read event handler, is set after `ngx_http_process_request_line()` to read the client request header.
+   `ngx_http_core_run_phases()` is called when the request header is completely read and parsed. This function runs request phases from `NGX_HTTP_POST_READ_PHASE` to `NGX_HTTP_CONTENT_PHASE`. The last phase is intended to generate a response and pass it along the filter chain. The response is not necessarily sent to the client at this phase. It might remain buffered and be sent at the finalization stage.
+   `ngx_http_finalize_request()` is usually called when the request has generated all the output or produced an error. In the latter case an appropriate error page is looked up and used as the response. If the response is not completely sent to the client by this point, an HTTP writer `ngx_http_writer()` is activated to finish sending outstanding data.
+   `ngx_http_finalize_connection()` is called when the complete response has been sent to the client and the request can be destroyed. If the client connection keepalive feature is enabled, `ngx_http_set_keepalive()` is called, which destroys the current request and waits for the next request on the connection. Otherwise, `ngx_http_close_request()` destroys both the request and the connection.

#### Request

For each client HTTP request the `ngx_http_request_t` object is created. Some of the fields of this object are:

+   `connection` — Pointer to a `ngx_connection_t` client connection object. Several requests can reference the same connection object at the same time - one main request and its subrequests. After a request is deleted, a new request can be created on the same connection.
    
    Note that for HTTP connections `ngx_connection_t`'s `data` field points back to the request. Such requests are called active, as opposed to the other requests tied to the connection. An active request is used to handle client connection events and is allowed to output its response to the client. Normally, each request becomes active at some point so that it can send its output.
    
+   `ctx` — Array of HTTP module contexts. Each module of type `NGX_HTTP_MODULE` can store any value (normally, a pointer to a structure) in the request. The value is stored in the `ctx` array at the module's `ctx_index` position. The following macros provide a convenient way to get and set request contexts:
    
    +   `ngx_http_get_module_ctx(r, module)` — Returns the `module`'s context
    +   `ngx_http_set_ctx(r, c, module)` — Sets `c` as the `module`'s context
+   `main_conf`, `srv_conf`, `loc_conf` — Arrays of current request configurations. Configurations are stored at the module's `ctx_index` positions.
+   `read_event_handler`, `write_event_handler` - Read and write event handlers for the request. Normally, both the read and write event handlers for an HTTP connection are set to `ngx_http_request_handler()`. This function calls the `read_event_handler` and `write_event_handler` handlers for the currently active request.
+   `cache` — Request cache object for caching the upstream response.
+   `upstream` — Request upstream object for proxying.
+   `pool` — Request pool. The request object itself is allocated in this pool, which is destroyed when the request is deleted. For allocations that need to be available throughout the client connection's lifetime, use `ngx_connection_t`'s pool instead.
+   `header_in` — Buffer into which the client HTTP request header is read.
+   `headers_in`, `headers_out` — Input and output HTTP headers objects. Both objects contain the `headers` field of type `ngx_list_t` for keeping the raw list of headers. In addition to that, specific headers are available for getting and setting as separate fields, for example `content_length_n`, `status` etc.
+   `request_body` — Client request body object.
+   `start_sec`, `start_msec` — Time point when the request was created, used for tracking request duration.
+   `method`, `method_name` — Numeric and text representation of the client HTTP request method. Numeric values for methods are defined in `src/http/ngx_http_request.h` with the macros `NGX_HTTP_GET`, `NGX_HTTP_HEAD`, `NGX_HTTP_POST`, etc.
+   `http_protocol` — Client HTTP protocol version in its original text form (“HTTP/1.0”, “HTTP/1.1” etc).
+   `http_version` — Client HTTP protocol version in numeric form (`NGX_HTTP_VERSION_10`, `NGX_HTTP_VERSION_11`, etc.).
+   `http_major`, `http_minor` — Client HTTP protocol version in numeric form split into major and minor parts.
+   `request_line`, `unparsed_uri` — Request line and URI in the original client request.
+   `uri`, `args`, `exten` — URI, arguments and file extension for the current request. The URI value here might differ from the original URI sent by the client due to normalization. Throughout request processing, these values can change as internal redirects are performed.
+   `main` — Pointer to a main request object. This object is created to process a client HTTP request, as opposed to subrequests, which are created to perform a specific subtask within the main request.
+   `parent` — Pointer to the parent request of a subrequest.
+   `postponed` — List of output buffers and subrequests, in the order in which they are sent and created. The list is used by the postpone filter to provide consistent request output when parts of it are created by subrequests.
+   `post_subrequest` — Pointer to a handler with the context to be called when a subrequest gets finalized. Unused for main requests.
+   `posted_requests` — List of requests to be started or resumed, which is done by calling the request's `write_event_handler`. Normally, this handler holds the request main function, which at first runs request phases and then produces the output.
    
    A request is usually posted by the `ngx_http_post_request(r, NULL)` call. It is always posted to the main request `posted_requests` list. The function `ngx_http_run_posted_requests(c)` runs all requests that are posted in the main request of the passed connection's active request. All event handlers call `ngx_http_run_posted_requests`, which can lead to new posted requests. Normally, it is called after invoking a request's read or write handler.
    
+   `phase_handler` — Index of current request phase.
+   `ncaptures`, `captures`, `captures_data` — Regex captures produced by the last regex match of the request. A regex match can occur at a number of places during request processing: map lookup, server lookup by SNI or HTTP Host, rewrite, proxy\_redirect, etc. Captures produced by a lookup are stored in the above mentioned fields. The field `ncaptures` holds the number of captures, `captures` holds captures boundaries and `captures_data` holds the string against which the regex was matched and which is used to extract captures. After each new regex match, request captures are reset to hold new values.
+   `count` — Request reference counter. The field only makes sense for the main request. Increasing the counter is done by simple `r->main->count++`. To decrease the counter, call `ngx_http_finalize_request(r, rc)`. Creating of a subrequest and running the request body read process both increment the counter.
+   `subrequests` — Current subrequest nesting level. Each subrequest inherits its parent's nesting level, decreased by one. An error is generated if the value reaches zero. The value for the main request is defined by the `NGX_HTTP_MAX_SUBREQUESTS` constant.
+   `uri_changes` — Number of URI changes remaining for the request. The total number of times a request can change its URI is limited by the `NGX_HTTP_MAX_URI_CHANGES` constant. With each change the value is decremented until it reaches zero, at which time an error is generated. Rewrites and internal redirects to normal or named locations are considered URI changes.
+   `blocked` — Counter of blocks held on the request. While this value is non-zero, the request cannot be terminated. Currently, this value is increased by pending AIO operations (POSIX AIO and thread operations) and active cache lock.
+   `buffered` — Bitmask showing which modules have buffered the output produced by the request. A number of filters can buffer output; for example, sub\_filter can buffer data because of a partial string match, copy filter can buffer data because of the lack of free output buffers etc. As long as this value is non-zero, the request is not finalized pending the flush.
+   `header_only` — Flag indicating that the output does not require a body. For example, this flag is used by HTTP HEAD requests.
+   `keepalive` — Flag indicating whether client connection keepalive is supported. The value is inferred from the HTTP version and the value of the “Connection” header.
    
+   `header_sent` — Flag indicating that the output header has already been sent by the request.
+   `internal` — Flag indicating that the current request is internal. To enter the internal state, a request must pass through an internal redirect or be a subrequest. Internal requests are allowed to enter internal locations.
+   `allow_ranges` — Flag indicating that a partial response can be sent to the client, as requested by the HTTP Range header.
+   `subrequest_ranges` — Flag indicating that a partial response can be sent while a subrequest is being processed.
+   `single_range` — Flag indicating that only a single continuous range of output data can be sent to the client. This flag is usually set when sending a stream of data, for example from a proxied server, and the entire response is not available in one buffer.
+   `main_filter_need_in_memory`, `filter_need_in_memory` — Flags requesting that the output produced in memory buffers rather than files. This is a signal to the copy filter to read data from file buffers even if sendfile is enabled. The difference between the two flags is the location of the filter modules that set them. Filters called before the postpone filter in the filter chain set `filter_need_in_memory`, requesting that only the current request output come in memory buffers. Filters called later in the filter chain set `main_filter_need_in_memory`, requesting that both the main request and all subrequests read files in memory while sending output.
+   `filter_need_temporary` — Flag requesting that the request output be produced in temporary buffers, but not in readonly memory buffers or file buffers. This is used by filters which may change output directly in the buffers where it's sent.

#### Configuration

Each HTTP module can have three types of configuration:

+   Main configuration — Applies to the entire `http` block. Functions as global settings for a module.
+   Server configuration — Applies to a single `server` block. Functions as server-specific settings for a module.
+   Location configuration — Applies to a single `location`, `if` or `limit_except` block. Functions as location-specific settings for a module.

Configuration structures are created at the nginx configuration stage by calling functions, which allocate the structures, initialize them and merge them. The following example shows how to create a simple location configuration for a module. The configuration has one setting, `foo`, of type unsigned integer.

> typedef struct {
>     ngx\_uint\_t  foo;
> } ngx\_http\_foo\_loc\_conf\_t;
> 
> 
> static ngx\_http\_module\_t  ngx\_http\_foo\_module\_ctx = {
>     NULL,                                  /\* preconfiguration \*/
>     NULL,                                  /\* postconfiguration \*/
> 
>     NULL,                                  /\* create main configuration \*/
>     NULL,                                  /\* init main configuration \*/
> 
>     NULL,                                  /\* create server configuration \*/
>     NULL,                                  /\* merge server configuration \*/
> 
>     ngx\_http\_foo\_create\_loc\_conf,          /\* create location configuration \*/
>     ngx\_http\_foo\_merge\_loc\_conf            /\* merge location configuration \*/
> };
> 
> 
> static void \*
> ngx\_http\_foo\_create\_loc\_conf(ngx\_conf\_t \*cf)
> {
>     ngx\_http\_foo\_loc\_conf\_t  \*conf;
> 
>     conf = ngx\_pcalloc(cf->pool, sizeof(ngx\_http\_foo\_loc\_conf\_t));
>     if (conf == NULL) {
>         return NULL;
>     }
> 
>     conf->foo = NGX\_CONF\_UNSET\_UINT;
> 
>     return conf;
> }
> 
> 
> static char \*
> ngx\_http\_foo\_merge\_loc\_conf(ngx\_conf\_t \*cf, void \*parent, void \*child)
> {
>     ngx\_http\_foo\_loc\_conf\_t \*prev = parent;
>     ngx\_http\_foo\_loc\_conf\_t \*conf = child;
> 
>     ngx\_conf\_merge\_uint\_value(conf->foo, prev->foo, 1);
> }

As seen in the example, the `ngx_http_foo_create_loc_conf()` function creates a new configuration structure, and `ngx_http_foo_merge_loc_conf()` merges a configuration with configuration from a higher level. In fact, server and location configuration do not exist only at the server and location levels, but are also created for all levels above them. Specifically, a server configuration is also created at the main level and location configurations are created at the main, server, and location levels. These configurations make it possible to specify server- and location-specific settings at any level of an nginx configuration file. Eventually configurations are merged down. A number of macros like `NGX_CONF_UNSET` and `NGX_CONF_UNSET_UINT` are provided for indicating a missing setting and ignoring it while merging. Standard nginx merge macros like `ngx_conf_merge_value()` and `ngx_conf_merge_uint_value()` provide a convenient way to merge a setting and set the default value if none of the configurations provided an explicit value. For complete list of macros for different types, see `src/core/ngx_conf_file.h`.

The following macros are available. for accessing configuration for HTTP modules at configuration time. They all take `ngx_conf_t` reference as the first argument.

+   `ngx_http_conf_get_module_main_conf(cf, module)`
+   `ngx_http_conf_get_module_srv_conf(cf, module)`
+   `ngx_http_conf_get_module_loc_conf(cf, module)`

The following example gets a pointer to a location configuration of standard nginx core module [ngx\_http\_core\_module](../http/ngx_http_core_module.html) and replaces the location content handler kept in the `handler` field of the structure.

> static ngx\_int\_t ngx\_http\_foo\_handler(ngx\_http\_request\_t \*r);
> 
> 
> static ngx\_command\_t  ngx\_http\_foo\_commands\[\] = {
> 
>     { ngx\_string("foo"),
>       NGX\_HTTP\_LOC\_CONF|NGX\_CONF\_NOARGS,
>       ngx\_http\_foo,
>       0,
>       0,
>       NULL },
> 
>       ngx\_null\_command
> };
> 
> 
> static char \*
> ngx\_http\_foo(ngx\_conf\_t \*cf, ngx\_command\_t \*cmd, void \*conf)
> {
>     ngx\_http\_core\_loc\_conf\_t  \*clcf;
> 
>     clcf = ngx\_http\_conf\_get\_module\_loc\_conf(cf, ngx\_http\_core\_module);
>     clcf->handler = ngx\_http\_bar\_handler;
> 
>     return NGX\_CONF\_OK;
> }

The following macros are available for accessing configuration for HTTP modules at runtime.

+   `ngx_http_get_module_main_conf(r, module)`
+   `ngx_http_get_module_srv_conf(r, module)`
+   `ngx_http_get_module_loc_conf(r, module)`

These macros receive a reference to an HTTP request `ngx_http_request_t`. The main configuration of a request never changes. Server configuration can change from the default after the virtual server for the request is chosen. Location configuration selected for processing a request can change multiple times as a result of a rewrite operation or internal redirect. The following example shows how to access a module's HTTP configuration at runtime.

> static ngx\_int\_t
> ngx\_http\_foo\_handler(ngx\_http\_request\_t \*r)
> {
>     ngx\_http\_foo\_loc\_conf\_t  \*flcf;
> 
>     flcf = ngx\_http\_get\_module\_loc\_conf(r, ngx\_http\_foo\_module);
> 
>     ...
> }

#### Phases

Each HTTP request passes through a sequence of phases. In each phase a distinct type of processing is performed on the request. Module-specific handlers can be registered in most phases, and many standard nginx modules register their phase handlers as a way to get called at a specific stage of request processing. Phases are processed successively and the phase handlers are called once the request reaches the phase. Following is the list of nginx HTTP phases.

+   `NGX_HTTP_POST_READ_PHASE` — First phase. The [ngx\_http\_realip\_module](../http/ngx_http_realip_module.html) registers its handler at this phase to enable substitution of client addresses before any other module is invoked.
+   `NGX_HTTP_SERVER_REWRITE_PHASE` — Phase where rewrite directives defined in a `server` block (but outside a `location` block) are processed. The [ngx\_http\_rewrite\_module](../http/ngx_http_rewrite_module.html) installs its handler at this phase.
+   `NGX_HTTP_FIND_CONFIG_PHASE` — Special phase where a location is chosen based on the request URI. Before this phase, the default location for the relevant virtual server is assigned to the request, and any module requesting a location configuration receives the configuration for the default server location. This phase assigns a new location to the request. No additional handlers can be registered at this phase.
+   `NGX_HTTP_REWRITE_PHASE` — Same as `NGX_HTTP_SERVER_REWRITE_PHASE`, but for rewrite rules defined in the location, chosen in the previous phase.
+   `NGX_HTTP_POST_REWRITE_PHASE` — Special phase where the request is redirected to a new location if its URI changed during a rewrite. This is implemented by the request going through the `NGX_HTTP_FIND_CONFIG_PHASE` again. No additional handlers can be registered at this phase.
+   `NGX_HTTP_PREACCESS_PHASE` — A common phase for different types of handlers, not associated with access control. The standard nginx modules [ngx\_http\_limit\_conn\_module](../http/ngx_http_limit_conn_module.html) and [ngx\_http\_limit\_req\_module](../http/ngx_http_limit_req_module.html) register their handlers at this phase.
+   `NGX_HTTP_ACCESS_PHASE` — Phase where it is verified that the client is authorized to make the request. Standard nginx modules such as [ngx\_http\_access\_module](../http/ngx_http_access_module.html) and [ngx\_http\_auth\_basic\_module](../http/ngx_http_auth_basic_module.html) register their handlers at this phase. By default the client must pass the authorization check of all handlers registered at this phase for the request to continue to the next phase. The [satisfy](../http/ngx_http_core_module.html#satisfy) directive, can be used to permit processing to continue if any of the phase handlers authorizes the client.
+   `NGX_HTTP_POST_ACCESS_PHASE` — Special phase where the [satisfy any](../http/ngx_http_core_module.html#satisfy) directive is processed. If some access phase handlers denied access and none explicitly allowed it, the request is finalized. No additional handlers can be registered at this phase.
+   `NGX_HTTP_PRECONTENT_PHASE` — Phase for handlers to be called prior to generating content. Standard modules such as [ngx\_http\_try\_files\_module](../http/ngx_http_core_module.html#try_files) and [ngx\_http\_mirror\_module](../http/ngx_http_mirror_module.html) register their handlers at this phase.
+   `NGX_HTTP_CONTENT_PHASE` — Phase where the response is normally generated. Multiple nginx standard modules register their handlers at this phase, including [ngx\_http\_index\_module](../http/ngx_http_index_module.html) or `ngx_http_static_module`. They are called sequentially until one of them produces the output. It's also possible to set content handlers on a per-location basis. If the [ngx\_http\_core\_module](../http/ngx_http_core_module.html)'s location configuration has `handler` set, it is called as the content handler and the handlers installed at this phase are ignored.
+   `NGX_HTTP_LOG_PHASE` — Phase where request logging is performed. Currently, only the [ngx\_http\_log\_module](../http/ngx_http_log_module.html) registers its handler at this stage for access logging. Log phase handlers are called at the very end of request processing, right before freeing the request.

Following is the example of a preaccess phase handler.

> static ngx\_http\_module\_t  ngx\_http\_foo\_module\_ctx = {
>     NULL,                                  /\* preconfiguration \*/
>     ngx\_http\_foo\_init,                     /\* postconfiguration \*/
> 
>     NULL,                                  /\* create main configuration \*/
>     NULL,                                  /\* init main configuration \*/
> 
>     NULL,                                  /\* create server configuration \*/
>     NULL,                                  /\* merge server configuration \*/
> 
>     NULL,                                  /\* create location configuration \*/
>     NULL                                   /\* merge location configuration \*/
> };
> 
> 
> static ngx\_int\_t
> ngx\_http\_foo\_handler(ngx\_http\_request\_t \*r)
> {
>     ngx\_table\_elt\_t  \*ua;
> 
>     ua = r->headers\_in.user\_agent;
> 
>     if (ua == NULL) {
>         return NGX\_DECLINED;
>     }
> 
>     /\* reject requests with "User-Agent: foo" \*/
>     if (ua->value.len == 3 && ngx\_strncmp(ua->value.data, "foo", 3) == 0) {
>         return NGX\_HTTP\_FORBIDDEN;
>     }
> 
>     return NGX\_DECLINED;
> }
> 
> 
> static ngx\_int\_t
> ngx\_http\_foo\_init(ngx\_conf\_t \*cf)
> {
>     ngx\_http\_handler\_pt        \*h;
>     ngx\_http\_core\_main\_conf\_t  \*cmcf;
> 
>     cmcf = ngx\_http\_conf\_get\_module\_main\_conf(cf, ngx\_http\_core\_module);
> 
>     h = ngx\_array\_push(&cmcf->phases\[NGX\_HTTP\_PREACCESS\_PHASE\].handlers);
>     if (h == NULL) {
>         return NGX\_ERROR;
>     }
> 
>     \*h = ngx\_http\_foo\_handler;
> 
>     return NGX\_OK;
> }

Phase handlers are expected to return specific codes:

+   `NGX_OK` — Proceed to the next phase.
+   `NGX_DECLINED` — Proceed to the next handler of the current phase. If the current handler is the last in the current phase, move to the next phase.
+   `NGX_AGAIN`, `NGX_DONE` — Suspend phase handling until some future event which can be an asynchronous I/O operation or just a delay, for example. It is assumed, that phase handling will be resumed later by calling `ngx_http_core_run_phases()`.
+   Any other value returned by the phase handler is treated as a request finalization code, in particular, an HTTP response code. The request is finalized with the code provided.

For some phases, return codes are treated in a slightly different way. At the content phase, any return code other that `NGX_DECLINED` is considered a finalization code. Any return code from the location content handlers is considered a finalization code. At the access phase, in [satisfy any](../http/ngx_http_core_module.html#satisfy) mode, any return code other than `NGX_OK`, `NGX_DECLINED`, `NGX_AGAIN`, `NGX_DONE` is considered a denial. If no subsequent access handlers allow or deny access with a different code, the denial code will become the finalization code.

#### Variables

#### Accessing existing variables

Variables can be referenced by index (this is the most common method) or name (see [below](#http_creating_variables)). The index is created at configuration stage, when a variable is added to the configuration. To obtain the variable index, use `ngx_http_get_variable_index()`:

> ngx\_str\_t  name;  /\* ngx\_string("foo") \*/
> ngx\_int\_t  index;
> 
> index = ngx\_http\_get\_variable\_index(cf, &name);

Here, `cf` is a pointer to nginx configuration and `name` points to a string containing the variable name. The function returns `NGX_ERROR` on error or a valid index otherwise, which is typically stored somewhere in the module's configuration for future use.

All HTTP variables are evaluated in the context of a given HTTP request, and results are specific to and cached in that HTTP request. All functions that evaluate variables return the `ngx_http_variable_value_t` type, representing the variable value:

> typedef ngx\_variable\_value\_t  ngx\_http\_variable\_value\_t;
> 
> typedef struct {
>     unsigned    len:28;
> 
>     unsigned    valid:1;
>     unsigned    no\_cacheable:1;
>     unsigned    not\_found:1;
>     unsigned    escape:1;
> 
>     u\_char     \*data;
> } ngx\_variable\_value\_t;

where:

+   `len` — The length of the value
+   `data` — The value itself
+   `valid` — The value is valid
+   `not_found` — The variable was not found and thus the `data` and `len` fields are irrelevant; this can happen, for example, with variables like `$arg_foo` when a corresponding argument was not passed in a request
+   `no_cacheable` — Do not cache result
+   `escape` — Used internally by the logging module to mark values that require escaping on output.

The `ngx_http_get_flushed_variable()` and `ngx_http_get_indexed_variable()` functions are used to obtain the value of a variable. They have the same interface - accepting an HTTP request `r` as a context for evaluating the variable and an `index` that identifies it. An example of typical usage:

> ngx\_http\_variable\_value\_t  \*v;
> 
> v = ngx\_http\_get\_flushed\_variable(r, index);
> 
> if (v == NULL || v->not\_found) {
>     /\* we failed to get value or there is no such variable, handle it \*/
>     return NGX\_ERROR;
> }
> 
> /\* some meaningful value is found \*/

The difference between functions is that the `ngx_http_get_indexed_variable()` returns a cached value and `ngx_http_get_flushed_variable()` flushes the cache for non-cacheable variables.

Some modules, such as SSI and Perl, need to deal with variables for which the name is not known at configuration time. An index therefore cannot be used to access them, but the `ngx_http_get_variable(r, name, key)` function is available. It searches for a variable with a given `name` and its hash `key` derived from the name.

#### Creating variables

To create a variable, use the `ngx_http_add_variable()` function. It takes as arguments a configuration (where the variable is registered), the variable name and flags that control the function's behaviour:

+   `NGX_HTTP_VAR_CHANGEABLE` — Enables redefinition of the variable: there is no conflict if another module defines a variable with the same name. This allows the [set](../http/ngx_http_rewrite_module.html#set) directive to override variables.
+   `NGX_HTTP_VAR_NOCACHEABLE` — Disables caching, which is useful for variables such as `$time_local`.
+   `NGX_HTTP_VAR_NOHASH` — Indicates that this variable is only accessible by index, not by name. This is a small optimization for use when it is known that the variable is not needed in modules like SSI or Perl.
+   `NGX_HTTP_VAR_PREFIX` — The name of the variable is a prefix. In this case, a handler must implement additional logic to obtain the value of a specific variable. For example, all “`arg_`” variables are processed by the same handler, which performs lookup in request arguments and returns the value of a specific argument.

The function returns NULL in case of error or a pointer to `ngx_http_variable_t` otherwise:

> struct ngx\_http\_variable\_s {
>     ngx\_str\_t                     name;
>     ngx\_http\_set\_variable\_pt      set\_handler;
>     ngx\_http\_get\_variable\_pt      get\_handler;
>     uintptr\_t                     data;
>     ngx\_uint\_t                    flags;
>     ngx\_uint\_t                    index;
> };

The `get` and `set` handlers are called to obtain or set the variable value, `data` is passed to variable handlers, and `index` holds assigned variable index used to reference the variable.

Usually, a null-terminated static array of `ngx_http_variable_t` structures is created by a module and processed at the preconfiguration stage to add variables into the configuration, for example:

> static ngx\_http\_variable\_t  ngx\_http\_foo\_vars\[\] = {
> 
>     { ngx\_string("foo\_v1"), NULL, ngx\_http\_foo\_v1\_variable, 0, 0, 0 },
> 
>       ngx\_http\_null\_variable
> };
> 
> static ngx\_int\_t
> ngx\_http\_foo\_add\_variables(ngx\_conf\_t \*cf)
> {
>     ngx\_http\_variable\_t  \*var, \*v;
> 
>     for (v = ngx\_http\_foo\_vars; v->name.len; v++) {
>         var = ngx\_http\_add\_variable(cf, &v->name, v->flags);
>         if (var == NULL) {
>             return NGX\_ERROR;
>         }
> 
>         var->get\_handler = v->get\_handler;
>         var->data = v->data;
>     }
> 
>     return NGX\_OK;
> }

This function in the example is used to initialize the `preconfiguration` field of the HTTP module context and is called before the parsing of HTTP configuration, so that the parser can refer to these variables.

The `get` handler is responsible for evaluating a variable in the context of a specific request, for example:

> static ngx\_int\_t
> ngx\_http\_variable\_connection(ngx\_http\_request\_t \*r,
>     ngx\_http\_variable\_value\_t \*v, uintptr\_t data)
> {
>     u\_char  \*p;
> 
>     p = ngx\_pnalloc(r->pool, NGX\_ATOMIC\_T\_LEN);
>     if (p == NULL) {
>         return NGX\_ERROR;
>     }
> 
>     v->len = ngx\_sprintf(p, "%uA", r->connection->number) - p;
>     v->valid = 1;
>     v->no\_cacheable = 0;
>     v->not\_found = 0;
>     v->data = p;
> 
>     return NGX\_OK;
> }

It returns `NGX_ERROR` in case of internal error (for example, failed memory allocation) or `NGX_OK` otherwise. To learn the status of variable evaluation, inspect the flags in `ngx_http_variable_value_t` (see the description [above](#http_existing_variables)).

The `set` handler allows setting the property referenced by the variable. For example, the set handler of the `$limit_rate` variable modifies the request's `limit_rate` field:

> ...
> { ngx\_string("limit\_rate"), ngx\_http\_variable\_request\_set\_size,
>   ngx\_http\_variable\_request\_get\_size,
>   offsetof(ngx\_http\_request\_t, limit\_rate),
>   NGX\_HTTP\_VAR\_CHANGEABLE|NGX\_HTTP\_VAR\_NOCACHEABLE, 0 },
> ...
> 
> static void
> ngx\_http\_variable\_request\_set\_size(ngx\_http\_request\_t \*r,
>     ngx\_http\_variable\_value\_t \*v, uintptr\_t data)
> {
>     ssize\_t    s, \*sp;
>     ngx\_str\_t  val;
> 
>     val.len = v->len;
>     val.data = v->data;
> 
>     s = ngx\_parse\_size(&val);
> 
>     if (s == NGX\_ERROR) {
>         ngx\_log\_error(NGX\_LOG\_ERR, r->connection->log, 0,
>                       "invalid size \\"%V\\"", &val);
>         return;
>     }
> 
>     sp = (ssize\_t \*) ((char \*) r + data);
> 
>     \*sp = s;
> 
>     return;
> }

#### Complex values

A complex value, despite its name, provides an easy way to evaluate expressions which can contain text, variables, and their combination.

The complex value description in `ngx_http_compile_complex_value` is compiled at the configuration stage into `ngx_http_complex_value_t` which is used at runtime to obtain results of expression evaluation.

> ngx\_str\_t                         \*value;
> ngx\_http\_complex\_value\_t           cv;
> ngx\_http\_compile\_complex\_value\_t   ccv;
> 
> value = cf->args->elts; /\* directive arguments \*/
> 
> ngx\_memzero(&ccv, sizeof(ngx\_http\_compile\_complex\_value\_t));
> 
> ccv.cf = cf;
> ccv.value = &value\[1\];
> ccv.complex\_value = &cv;
> ccv.zero = 1;
> ccv.conf\_prefix = 1;
> 
> if (ngx\_http\_compile\_complex\_value(&ccv) != NGX\_OK) {
>     return NGX\_CONF\_ERROR;
> }

Here, `ccv` holds all parameters that are required to initialize the complex value `cv`:

+   `cf` — Configuration pointer
+   `value` — String to be parsed (input)
+   `complex_value` — Compiled value (output)
+   `zero` — Flag that enables zero-terminating value
+   `conf_prefix` — Prefixes the result with the configuration prefix (the directory where nginx is currently looking for configuration)
+   `root_prefix` — Prefixes the result with the root prefix (the normal nginx installation prefix)

The `zero` flag is useful when results are to be passed to libraries that require zero-terminated strings, and prefixes are handy when dealing with filenames.

Upon successful compilation, `cv.lengths` contains information about the presence of variables in the expression. The NULL value means that the expression contained static text only, and so can be stored in a simple string rather than as a complex value.

The `ngx_http_set_complex_value_slot()` is a convenient function used to initialize a complex value completely in the directive declaration itself.

At runtime, a complex value can be calculated using the `ngx_http_complex_value()` function:

> ngx\_str\_t  res;
> 
> if (ngx\_http\_complex\_value(r, &cv, &res) != NGX\_OK) {
>     return NGX\_ERROR;
> }

Given the request `r` and previously compiled value `cv`, the function evaluates the expression and writes the result to `res`.

#### Request redirection

An HTTP request is always connected to a location via the `loc_conf` field of the `ngx_http_request_t` structure. This means that at any point the location configuration of any module can be retrieved from the request by calling `ngx_http_get_module_loc_conf(r, module)`. Request location can change several times during the request's lifetime. Initially, a default server location of the default server is assigned to a request. If the request switches to a different server (chosen by the HTTP “Host” header or SSL SNI extension), the request switches to the default location of that server as well. The next change of the location takes place at the `NGX_HTTP_FIND_CONFIG_PHASE` request phase. At this phase a location is chosen by request URI among all non-named locations configured for the server. The [ngx\_http\_rewrite\_module](../http/ngx_http_rewrite_module.html) can change the request URI at the `NGX_HTTP_REWRITE_PHASE` request phase as a result of the [rewrite](../http/ngx_http_rewrite_module.html#rewrite) directive and send the request back to the `NGX_HTTP_FIND_CONFIG_PHASE` phase for selection of a new location based on the new URI.

It is also possible to redirect a request to a new location at any point by calling one of `ngx_http_internal_redirect(r, uri, args)` or `ngx_http_named_location(r, name)`.

The `ngx_http_internal_redirect(r, uri, args)` function changes the request URI and returns the request to the `NGX_HTTP_SERVER_REWRITE_PHASE` phase. The request proceeds with a server default location. Later at `NGX_HTTP_FIND_CONFIG_PHASE` a new location is chosen based on the new request URI.

The following example performs an internal redirect with the new request arguments.

> ngx\_int\_t
> ngx\_http\_foo\_redirect(ngx\_http\_request\_t \*r)
> {
>     ngx\_str\_t  uri, args;
> 
>     ngx\_str\_set(&uri, "/foo");
>     ngx\_str\_set(&args, "bar=1");
> 
>     return ngx\_http\_internal\_redirect(r, &uri, &args);
> }

The function `ngx_http_named_location(r, name)` redirects a request to a named location. The name of the location is passed as the argument. The location is looked up among all named locations of the current server, after which the requests switches to the `NGX_HTTP_REWRITE_PHASE` phase.

The following example performs a redirect to a named location @foo.

> ngx\_int\_t
> ngx\_http\_foo\_named\_redirect(ngx\_http\_request\_t \*r)
> {
>     ngx\_str\_t  name;
> 
>     ngx\_str\_set(&name, "foo");
> 
>     return ngx\_http\_named\_location(r, &name);
> }

Both functions - `ngx_http_internal_redirect(r, uri, args)` and `ngx_http_named_location(r, name)` can be called when nginx modules have already stored some contexts in a request's `ctx` field. It's possible for these contexts to become inconsistent with the new location configuration. To prevent inconsistency, all request contexts are erased by both redirect functions.

Calling `ngx_http_internal_redirect(r, uri, args)` or `ngx_http_named_location(r, name)` increases the request `count`. For consistent request reference counting, call `ngx_http_finalize_request(r, NGX_DONE)` after redirecting the request. This will finalize current request code path and decrease the counter.

Redirected and rewritten requests become internal and can access the [internal](../http/ngx_http_core_module.html#internal) locations. Internal requests have the `internal` flag set.

#### Subrequests

Subrequests are primarily used to insert output of one request into another, possibly mixed with other data. A subrequest looks like a normal request, but shares some data with its parent. In particular, all fields related to client input are shared because a subrequest does not receive any other input from the client. The request field `parent` for a subrequest contains a link to its parent request and is NULL for the main request. The field `main` contains a link to the main request in a group of requests.

A subrequest starts in the `NGX_HTTP_SERVER_REWRITE_PHASE` phase. It passes through the same subsequent phases as a normal request and is assigned a location based on its own URI.

The output header in a subrequest is always ignored. The `ngx_http_postpone_filter` places the subrequest's output body in the right position relative to other data produced by the parent request.

Subrequests are related to the concept of active requests. A request `r` is considered active if `c->data == r`, where `c` is the client connection object. At any given point, only the active request in a request group is allowed to output its buffers to the client. An inactive request can still send its output to the filter chain, but it does not pass beyond the `ngx_http_postpone_filter` and remains buffered by that filter until the request becomes active. Here are some rules of request activation:

+   Initially, the main request is active.
+   The first subrequest of an active request becomes active right after creation.
+   The `ngx_http_postpone_filter` activates the next request in the active request's subrequest list, once all data prior to that request are sent.
+   When a request is finalized, its parent is activated.

Create a subrequest by calling the function `ngx_http_subrequest(r, uri, args, psr, ps, flags)`, where `r` is the parent request, `uri` and `args` are the URI and arguments of the subrequest, `psr` is the output parameter, which receives the newly created subrequest reference, `ps` is a callback object for notifying the parent request that the subrequest is being finalized, and `flags` is bitmask of flags. The following flags are available:

+   `NGX_HTTP_SUBREQUEST_IN_MEMORY` - Output is not sent to the client, but rather stored in memory. The flag only affects subrequests which are processed by one of the proxying modules. After a subrequest is finalized its output is available in `r->out` of type `ngx_buf_t`.
+   `NGX_HTTP_SUBREQUEST_WAITED` - The subrequest's `done` flag is set even if the subrequest is not active when it is finalized. This subrequest flag is used by the SSI filter.
+   `NGX_HTTP_SUBREQUEST_CLONE` - The subrequest is created as a clone of its parent. It is started at the same location and proceeds from the same phase as the parent request.

The following example creates a subrequest with the URI of `/foo`.

> ngx\_int\_t            rc;
> ngx\_str\_t            uri;
> ngx\_http\_request\_t  \*sr;
> 
> ...
> 
> ngx\_str\_set(&uri, "/foo");
> 
> rc = ngx\_http\_subrequest(r, &uri, NULL, &sr, NULL, 0);
> if (rc == NGX\_ERROR) {
>     /\* error \*/
> }

This example clones the current request and sets a finalization callback for the subrequest.

> ngx\_int\_t
> ngx\_http\_foo\_clone(ngx\_http\_request\_t \*r)
> {
>     ngx\_http\_request\_t          \*sr;
>     ngx\_http\_post\_subrequest\_t  \*ps;
> 
>     ps = ngx\_palloc(r->pool, sizeof(ngx\_http\_post\_subrequest\_t));
>     if (ps == NULL) {
>         return NGX\_ERROR;
>     }
> 
>     ps->handler = ngx\_http\_foo\_subrequest\_done;
>     ps->data = "foo";
> 
>     return ngx\_http\_subrequest(r, &r->uri, &r->args, &sr, ps,
>                                NGX\_HTTP\_SUBREQUEST\_CLONE);
> }
> 
> 
> ngx\_int\_t
> ngx\_http\_foo\_subrequest\_done(ngx\_http\_request\_t \*r, void \*data, ngx\_int\_t rc)
> {
>     char  \*msg = (char \*) data;
> 
>     ngx\_log\_error(NGX\_LOG\_INFO, r->connection->log, 0,
>                   "done subrequest r:%p msg:%s rc:%i", r, msg, rc);
> 
>     return rc;
> }

Subrequests are normally created in a body filter, in which case their output can be treated like the output from any explicit request. This means that eventually the output of a subrequest is sent to the client, after all explicit buffers that are passed before subrequest creation and before any buffers that are passed after creation. This ordering is preserved even for large hierarchies of subrequests. The following example inserts output from a subrequest after all request data buffers, but before the final buffer with the `last_buf` flag.

> ngx\_int\_t
> ngx\_http\_foo\_body\_filter(ngx\_http\_request\_t \*r, ngx\_chain\_t \*in)
> {
>     ngx\_int\_t                   rc;
>     ngx\_buf\_t                  \*b;
>     ngx\_uint\_t                  last;
>     ngx\_chain\_t                \*cl, out;
>     ngx\_http\_request\_t         \*sr;
>     ngx\_http\_foo\_filter\_ctx\_t  \*ctx;
> 
>     ctx = ngx\_http\_get\_module\_ctx(r, ngx\_http\_foo\_filter\_module);
>     if (ctx == NULL) {
>         return ngx\_http\_next\_body\_filter(r, in);
>     }
> 
>     last = 0;
> 
>     for (cl = in; cl; cl = cl->next) {
>         if (cl->buf->last\_buf) {
>             cl->buf->last\_buf = 0;
>             cl->buf->last\_in\_chain = 1;
>             cl->buf->sync = 1;
>             last = 1;
>         }
>     }
> 
>     /\* Output explicit output buffers \*/
> 
>     rc = ngx\_http\_next\_body\_filter(r, in);
> 
>     if (rc == NGX\_ERROR || !last) {
>         return rc;
>     }
> 
>     /\*
>      \* Create the subrequest.  The output of the subrequest
>      \* will automatically be sent after all preceding buffers,
>      \* but before the last\_buf buffer passed later in this function.
>      \*/
> 
>     if (ngx\_http\_subrequest(r, ctx->uri, NULL, &sr, NULL, 0) != NGX\_OK) {
>         return NGX\_ERROR;
>     }
> 
>     ngx\_http\_set\_ctx(r, NULL, ngx\_http\_foo\_filter\_module);
> 
>     /\* Output the final buffer with the last\_buf flag \*/
> 
>     b = ngx\_calloc\_buf(r->pool);
>     if (b == NULL) {
>         return NGX\_ERROR;
>     }
> 
>     b->last\_buf = 1;
> 
>     out.buf = b;
>     out.next = NULL;
> 
>     return ngx\_http\_output\_filter(r, &out);
> }

A subrequest can also be created for other purposes than data output. For example, the [ngx\_http\_auth\_request\_module](../http/ngx_http_auth_request_module.html) module creates a subrequest at the `NGX_HTTP_ACCESS_PHASE` phase. To disable output at this point, the `header_only` flag is set on the subrequest. This prevents the subrequest body from being sent to the client. Note that the subrequest's header is never sent to the client. The result of the subrequest can be analyzed in the callback handler.

#### Request finalization

An HTTP request is finalized by calling the function `ngx_http_finalize_request(r, rc)`. It is usually finalized by the content handler after all output buffers are sent to the filter chain. At this point all of the output might not be sent to the client, with some of it remaining buffered somewhere along the filter chain. If it is, the `ngx_http_finalize_request(r, rc)` function automatically installs a special handler `ngx_http_writer(r)` to finish sending the output. A request is also finalized in case of an error or if a standard HTTP response code needs to be returned to the client.

The function `ngx_http_finalize_request(r, rc)` expects the following `rc` values:

+   `NGX_DONE` - Fast finalization. Decrement the request `count` and destroy the request if it reaches zero. The client connection can be used for more requests after the current request is destroyed.
+   `NGX_ERROR`, `NGX_HTTP_REQUEST_TIME_OUT` (`408`), `NGX_HTTP_CLIENT_CLOSED_REQUEST` (`499`) - Error finalization. Terminate the request as soon as possible and close the client connection.
+   `NGX_HTTP_CREATED` (`201`), `NGX_HTTP_NO_CONTENT` (`204`), codes greater than or equal to `NGX_HTTP_SPECIAL_RESPONSE` (`300`) - Special response finalization. For these values nginx either sends to the client a default response page for the code or performs the internal redirect to an [error\_page](../http/ngx_http_core_module.html#error_page) location if that is configured for the code.
+   Other codes are considered successful finalization codes and might activate the request writer to finish sending the response body. Once the body is completely sent, the request `count` is decremented. If it reaches zero, the request is destroyed, but the client connection can still be used for other requests. If `count` is positive, there are unfinished activities within the request, which will be finalized at a later point.

#### Request body

For dealing with the body of a client request, nginx provides the `ngx_http_read_client_request_body(r, post_handler)` and `ngx_http_discard_request_body(r)` functions. The first function reads the request body and makes it available via the `request_body` request field. The second function instructs nginx to discard (read and ignore) the request body. One of these functions must be called for every request. Normally, the content handler makes the call.

Reading or discarding the client request body from a subrequest is not allowed. It must always be done in the main request. When a subrequest is created, it inherits the parent's `request_body` object which can be used by the subrequest if the main request has previously read the request body.

The function `ngx_http_read_client_request_body(r, post_handler)` starts the process of reading the request body. When the body is completely read, the `post_handler` callback is called to continue processing the request. If the request body is missing or has already been read, the callback is called immediately. The function `ngx_http_read_client_request_body(r, post_handler)` allocates the `request_body` request field of type `ngx_http_request_body_t`. The field `bufs` of this object keeps the result as a buffer chain. The body can be saved in memory buffers or file buffers, if the capacity specified by the [client\_body\_buffer\_size](../http/ngx_http_core_module.html#client_body_buffer_size) directive is not enough to fit the entire body in memory.

The following example reads a client request body and returns its size.

> ngx\_int\_t
> ngx\_http\_foo\_content\_handler(ngx\_http\_request\_t \*r)
> {
>     ngx\_int\_t  rc;
> 
>     rc = ngx\_http\_read\_client\_request\_body(r, ngx\_http\_foo\_init);
> 
>     if (rc >= NGX\_HTTP\_SPECIAL\_RESPONSE) {
>         /\* error \*/
>         return rc;
>     }
> 
>     return NGX\_DONE;
> }
> 
> 
> void
> ngx\_http\_foo\_init(ngx\_http\_request\_t \*r)
> {
>     off\_t         len;
>     ngx\_buf\_t    \*b;
>     ngx\_int\_t     rc;
>     ngx\_chain\_t  \*in, out;
> 
>     if (r->request\_body == NULL) {
>         ngx\_http\_finalize\_request(r, NGX\_HTTP\_INTERNAL\_SERVER\_ERROR);
>         return;
>     }
> 
>     len = 0;
> 
>     for (in = r->request\_body->bufs; in; in = in->next) {
>         len += ngx\_buf\_size(in->buf);
>     }
> 
>     b = ngx\_create\_temp\_buf(r->pool, NGX\_OFF\_T\_LEN);
>     if (b == NULL) {
>         ngx\_http\_finalize\_request(r, NGX\_HTTP\_INTERNAL\_SERVER\_ERROR);
>         return;
>     }
> 
>     b->last = ngx\_sprintf(b->pos, "%O", len);
>     b->last\_buf = (r == r->main) ? 1 : 0;
>     b->last\_in\_chain = 1;
> 
>     r->headers\_out.status = NGX\_HTTP\_OK;
>     r->headers\_out.content\_length\_n = b->last - b->pos;
> 
>     rc = ngx\_http\_send\_header(r);
> 
>     if (rc == NGX\_ERROR || rc > NGX\_OK || r->header\_only) {
>         ngx\_http\_finalize\_request(r, rc);
>         return;
>     }
> 
>     out.buf = b;
>     out.next = NULL;
> 
>     rc = ngx\_http\_output\_filter(r, &out);
> 
>     ngx\_http\_finalize\_request(r, rc);
> }

The following fields of the request determine how the request body is read:

+   `request_body_in_single_buf` - Read the body to a single memory buffer.
+   `request_body_in_file_only` - Always read the body to a file, even if fits in the memory buffer.
+   `request_body_in_persistent_file` - Do not unlink the file immediately after creation. A file with this flag can be moved to another directory.
+   `request_body_in_clean_file` - Unlink the file when the request is finalized. This can be useful when a file was supposed to be moved to another directory but was not moved for some reason.
+   `request_body_file_group_access` - Enable group access to the file by replacing the default 0600 access mask with 0660.
+   `request_body_file_log_level` - Severity level at which to log file errors.
+   `request_body_no_buffering` - Read the request body without buffering.

The `request_body_no_buffering` flag enables the unbuffered mode of reading a request body. In this mode, after calling `ngx_http_read_client_request_body()`, the `bufs` chain might keep only a part of the body. To read the next part, call the `ngx_http_read_unbuffered_request_body(r)` function. The return value `NGX_AGAIN` and the request flag `reading_body` indicate that more data is available. If `bufs` is NULL after calling this function, there is nothing to read at the moment. The request callback `read_event_handler` will be called when the next part of request body is available.

#### Request body filters

After a request body part is read, it's passed to the request body filter chain by calling the first body filter handler stored in the `ngx_http_top_request_body_filter` variable. It's assumed that every body handler calls the next handler in the chain until the final handler `ngx_http_request_body_save_filter(r, cl)` is called. This handler collects the buffers in `r->request_body->bufs` and writes them to a file if necessary. The last request body buffer has nonzero `last_buf` flag.

If a filter is planning to delay data buffers, it should set the flag `r->request_body->filter_need_buffering` to `1` when called for the first time.

Following is an example of a simple request body filter that delays request body by one second.

> #include <ngx\_config.h>
> #include <ngx\_core.h>
> #include <ngx\_http.h>
> 
> 
> #define NGX\_HTTP\_DELAY\_BODY  1000
> 
> 
> typedef struct {
>     ngx\_event\_t   event;
>     ngx\_chain\_t  \*out;
> } ngx\_http\_delay\_body\_ctx\_t;
> 
> 
> static ngx\_int\_t ngx\_http\_delay\_body\_filter(ngx\_http\_request\_t \*r,
>     ngx\_chain\_t \*in);
> static void ngx\_http\_delay\_body\_cleanup(void \*data);
> static void ngx\_http\_delay\_body\_event\_handler(ngx\_event\_t \*ev);
> static ngx\_int\_t ngx\_http\_delay\_body\_init(ngx\_conf\_t \*cf);
> 
> 
> static ngx\_http\_module\_t  ngx\_http\_delay\_body\_module\_ctx = {
>     NULL,                          /\* preconfiguration \*/
>     ngx\_http\_delay\_body\_init,      /\* postconfiguration \*/
> 
>     NULL,                          /\* create main configuration \*/
>     NULL,                          /\* init main configuration \*/
> 
>     NULL,                          /\* create server configuration \*/
>     NULL,                          /\* merge server configuration \*/
> 
>     NULL,                          /\* create location configuration \*/
>     NULL                           /\* merge location configuration \*/
> };
> 
> 
> ngx\_module\_t  ngx\_http\_delay\_body\_filter\_module = {
>     NGX\_MODULE\_V1,
>     &ngx\_http\_delay\_body\_module\_ctx, /\* module context \*/
>     NULL,                          /\* module directives \*/
>     NGX\_HTTP\_MODULE,               /\* module type \*/
>     NULL,                          /\* init master \*/
>     NULL,                          /\* init module \*/
>     NULL,                          /\* init process \*/
>     NULL,                          /\* init thread \*/
>     NULL,                          /\* exit thread \*/
>     NULL,                          /\* exit process \*/
>     NULL,                          /\* exit master \*/
>     NGX\_MODULE\_V1\_PADDING
> };
> 
> 
> static ngx\_http\_request\_body\_filter\_pt   ngx\_http\_next\_request\_body\_filter;
> 
> 
> static ngx\_int\_t
> ngx\_http\_delay\_body\_filter(ngx\_http\_request\_t \*r, ngx\_chain\_t \*in)
> {
>     ngx\_int\_t                   rc;
>     ngx\_chain\_t                \*cl, \*ln;
>     ngx\_http\_cleanup\_t         \*cln;
>     ngx\_http\_delay\_body\_ctx\_t  \*ctx;
> 
>     ngx\_log\_debug0(NGX\_LOG\_DEBUG\_HTTP, r->connection->log, 0,
>                    "delay request body filter");
> 
>     ctx = ngx\_http\_get\_module\_ctx(r, ngx\_http\_delay\_body\_filter\_module);
> 
>     if (ctx == NULL) {
>         ctx = ngx\_pcalloc(r->pool, sizeof(ngx\_http\_delay\_body\_ctx\_t));
>         if (ctx == NULL) {
>             return NGX\_HTTP\_INTERNAL\_SERVER\_ERROR;
>         }
> 
>         ngx\_http\_set\_ctx(r, ctx, ngx\_http\_delay\_body\_filter\_module);
> 
>         r->request\_body->filter\_need\_buffering = 1;
>     }
> 
>     if (ngx\_chain\_add\_copy(r->pool, &ctx->out, in) != NGX\_OK) {
>         return NGX\_HTTP\_INTERNAL\_SERVER\_ERROR;
>     }
> 
>     if (!ctx->event.timedout) {
>         if (!ctx->event.timer\_set) {
> 
>             /\* cleanup to remove the timer in case of abnormal termination \*/
> 
>             cln = ngx\_http\_cleanup\_add(r, 0);
>             if (cln == NULL) {
>                 return NGX\_HTTP\_INTERNAL\_SERVER\_ERROR;
>             }
> 
>             cln->handler = ngx\_http\_delay\_body\_cleanup;
>             cln->data = ctx;
> 
>             /\* add timer \*/
> 
>             ctx->event.handler = ngx\_http\_delay\_body\_event\_handler;
>             ctx->event.data = r;
>             ctx->event.log = r->connection->log;
> 
>             ngx\_add\_timer(&ctx->event, NGX\_HTTP\_DELAY\_BODY);
>         }
> 
>         return ngx\_http\_next\_request\_body\_filter(r, NULL);
>     }
> 
>     rc = ngx\_http\_next\_request\_body\_filter(r, ctx->out);
> 
>     for (cl = ctx->out; cl; /\* void \*/) {
>         ln = cl;
>         cl = cl->next;
>         ngx\_free\_chain(r->pool, ln);
>     }
> 
>     ctx->out = NULL;
> 
>     return rc;
> }
> 
> 
> static void
> ngx\_http\_delay\_body\_cleanup(void \*data)
> {
>     ngx\_http\_delay\_body\_ctx\_t \*ctx = data;
> 
>     if (ctx->event.timer\_set) {
>         ngx\_del\_timer(&ctx->event);
>     }
> }
> 
> 
> static void
> ngx\_http\_delay\_body\_event\_handler(ngx\_event\_t \*ev)
> {
>     ngx\_connection\_t    \*c;
>     ngx\_http\_request\_t  \*r;
> 
>     r = ev->data;
>     c = r->connection;
> 
>     ngx\_log\_debug0(NGX\_LOG\_DEBUG\_HTTP, c->log, 0,
>                    "delay request body event");
> 
>     ngx\_post\_event(c->read, &ngx\_posted\_events);
> }
> 
> 
> static ngx\_int\_t
> ngx\_http\_delay\_body\_init(ngx\_conf\_t \*cf)
> {
>     ngx\_http\_next\_request\_body\_filter = ngx\_http\_top\_request\_body\_filter;
>     ngx\_http\_top\_request\_body\_filter = ngx\_http\_delay\_body\_filter;
> 
>     return NGX\_OK;
> }

#### Response

In nginx an HTTP response is produced by sending the response header followed by the optional response body. Both header and body are passed through a chain of filters and eventually get written to the client socket. An nginx module can install its handler into the header or body filter chain and process the output coming from the previous handler.

#### Response header

The `ngx_http_send_header(r)` function sends the output header. Do not call this function until `r->headers_out` contains all of the data required to produce the HTTP response header. The `status` field in `r->headers_out` must always be set. If the response status indicates that a response body follows the header, `content_length_n` can be set as well. The default value for this field is `-1`, which means that the body size is unknown. In this case, chunked transfer encoding is used. To output an arbitrary header, append the `headers` list.

> static ngx\_int\_t
> ngx\_http\_foo\_content\_handler(ngx\_http\_request\_t \*r)
> {
>     ngx\_int\_t         rc;
>     ngx\_table\_elt\_t  \*h;
> 
>     /\* send header \*/
> 
>     r->headers\_out.status = NGX\_HTTP\_OK;
>     r->headers\_out.content\_length\_n = 3;
> 
>     /\* X-Foo: foo \*/
> 
>     h = ngx\_list\_push(&r->headers\_out.headers);
>     if (h == NULL) {
>         return NGX\_ERROR;
>     }
> 
>     h->hash = 1;
>     ngx\_str\_set(&h->key, "X-Foo");
>     ngx\_str\_set(&h->value, "foo");
> 
>     rc = ngx\_http\_send\_header(r);
> 
>     if (rc == NGX\_ERROR || rc > NGX\_OK || r->header\_only) {
>         return rc;
>     }
> 
>     /\* send body \*/
> 
>     ...
> }

#### Header filters

The `ngx_http_send_header(r)` function invokes the header filter chain by calling the first header filter handler stored in the `ngx_http_top_header_filter` variable. It's assumed that every header handler calls the next handler in the chain until the final handler `ngx_http_header_filter(r)` is called. The final header handler constructs the HTTP response based on `r->headers_out` and passes it to the `ngx_http_writer_filter` for output.

To add a handler to the header filter chain, store its address in the global variable `ngx_http_top_header_filter` at configuration time. The previous handler address is normally stored in a static variable in a module and is called by the newly added handler before exiting.

The following example of a header filter module adds the HTTP header "`X-Foo: foo`" to every response with status `200`.

> #include <ngx\_config.h>
> #include <ngx\_core.h>
> #include <ngx\_http.h>
> 
> 
> static ngx\_int\_t ngx\_http\_foo\_header\_filter(ngx\_http\_request\_t \*r);
> static ngx\_int\_t ngx\_http\_foo\_header\_filter\_init(ngx\_conf\_t \*cf);
> 
> 
> static ngx\_http\_module\_t  ngx\_http\_foo\_header\_filter\_module\_ctx = {
>     NULL,                                   /\* preconfiguration \*/
>     ngx\_http\_foo\_header\_filter\_init,        /\* postconfiguration \*/
> 
>     NULL,                                   /\* create main configuration \*/
>     NULL,                                   /\* init main configuration \*/
> 
>     NULL,                                   /\* create server configuration \*/
>     NULL,                                   /\* merge server configuration \*/
> 
>     NULL,                                   /\* create location configuration \*/
>     NULL                                    /\* merge location configuration \*/
> };
> 
> 
> ngx\_module\_t  ngx\_http\_foo\_header\_filter\_module = {
>     NGX\_MODULE\_V1,
>     &ngx\_http\_foo\_header\_filter\_module\_ctx, /\* module context \*/
>     NULL,                                   /\* module directives \*/
>     NGX\_HTTP\_MODULE,                        /\* module type \*/
>     NULL,                                   /\* init master \*/
>     NULL,                                   /\* init module \*/
>     NULL,                                   /\* init process \*/
>     NULL,                                   /\* init thread \*/
>     NULL,                                   /\* exit thread \*/
>     NULL,                                   /\* exit process \*/
>     NULL,                                   /\* exit master \*/
>     NGX\_MODULE\_V1\_PADDING
> };
> 
> 
> static ngx\_http\_output\_header\_filter\_pt  ngx\_http\_next\_header\_filter;
> 
> 
> static ngx\_int\_t
> ngx\_http\_foo\_header\_filter(ngx\_http\_request\_t \*r)
> {
>     ngx\_table\_elt\_t  \*h;
> 
>     /\*
>      \* The filter handler adds "X-Foo: foo" header
>      \* to every HTTP 200 response
>      \*/
> 
>     if (r->headers\_out.status != NGX\_HTTP\_OK) {
>         return ngx\_http\_next\_header\_filter(r);
>     }
> 
>     h = ngx\_list\_push(&r->headers\_out.headers);
>     if (h == NULL) {
>         return NGX\_ERROR;
>     }
> 
>     h->hash = 1;
>     ngx\_str\_set(&h->key, "X-Foo");
>     ngx\_str\_set(&h->value, "foo");
> 
>     return ngx\_http\_next\_header\_filter(r);
> }
> 
> 
> static ngx\_int\_t
> ngx\_http\_foo\_header\_filter\_init(ngx\_conf\_t \*cf)
> {
>     ngx\_http\_next\_header\_filter = ngx\_http\_top\_header\_filter;
>     ngx\_http\_top\_header\_filter = ngx\_http\_foo\_header\_filter;
> 
>     return NGX\_OK;
> }

#### Response body

To send the response body, call the `ngx_http_output_filter(r, cl)` function. The function can be called multiple times. Each time, it sends a part of the response body in the form of a buffer chain. Set the `last_buf` flag in the last body buffer.

The following example produces a complete HTTP response with "foo" as its body. For the example to work as subrequest as well as a main request, the `last_in_chain` flag is set in the last buffer of the output. The `last_buf` flag is set only for the main request because the last buffer for a subrequest does not end the entire output.

> static ngx\_int\_t
> ngx\_http\_bar\_content\_handler(ngx\_http\_request\_t \*r)
> {
>     ngx\_int\_t     rc;
>     ngx\_buf\_t    \*b;
>     ngx\_chain\_t   out;
> 
>     /\* send header \*/
> 
>     r->headers\_out.status = NGX\_HTTP\_OK;
>     r->headers\_out.content\_length\_n = 3;
> 
>     rc = ngx\_http\_send\_header(r);
> 
>     if (rc == NGX\_ERROR || rc > NGX\_OK || r->header\_only) {
>         return rc;
>     }
> 
>     /\* send body \*/
> 
>     b = ngx\_calloc\_buf(r->pool);
>     if (b == NULL) {
>         return NGX\_ERROR;
>     }
> 
>     b->last\_buf = (r == r->main) ? 1 : 0;
>     b->last\_in\_chain = 1;
> 
>     b->memory = 1;
> 
>     b->pos = (u\_char \*) "foo";
>     b->last = b->pos + 3;
> 
>     out.buf = b;
>     out.next = NULL;
> 
>     return ngx\_http\_output\_filter(r, &out);
> }

#### Response body filters

The function `ngx_http_output_filter(r, cl)` invokes the body filter chain by calling the first body filter handler stored in the `ngx_http_top_body_filter` variable. It's assumed that every body handler calls the next handler in the chain until the final handler `ngx_http_write_filter(r, cl)` is called.

A body filter handler receives a chain of buffers. The handler is supposed to process the buffers and pass a possibly new chain to the next handler. It's worth noting that the chain links `ngx_chain_t` of the incoming chain belong to the caller, and must not be reused or changed. Right after the handler completes, the caller can use its output chain links to keep track of the buffers it has sent. To save the buffer chain or to substitute some buffers before passing to the next filter, a handler needs to allocate its own chain links.

Following is an example of a simple body filter that counts the number of bytes in the body. The result is available as the `$counter` variable which can be used in the access log.

> #include <ngx\_config.h>
> #include <ngx\_core.h>
> #include <ngx\_http.h>
> 
> 
> typedef struct {
>     off\_t  count;
> } ngx\_http\_counter\_filter\_ctx\_t;
> 
> 
> static ngx\_int\_t ngx\_http\_counter\_body\_filter(ngx\_http\_request\_t \*r,
>     ngx\_chain\_t \*in);
> static ngx\_int\_t ngx\_http\_counter\_variable(ngx\_http\_request\_t \*r,
>     ngx\_http\_variable\_value\_t \*v, uintptr\_t data);
> static ngx\_int\_t ngx\_http\_counter\_add\_variables(ngx\_conf\_t \*cf);
> static ngx\_int\_t ngx\_http\_counter\_filter\_init(ngx\_conf\_t \*cf);
> 
> 
> static ngx\_http\_module\_t  ngx\_http\_counter\_filter\_module\_ctx = {
>     ngx\_http\_counter\_add\_variables,        /\* preconfiguration \*/
>     ngx\_http\_counter\_filter\_init,          /\* postconfiguration \*/
> 
>     NULL,                                  /\* create main configuration \*/
>     NULL,                                  /\* init main configuration \*/
> 
>     NULL,                                  /\* create server configuration \*/
>     NULL,                                  /\* merge server configuration \*/
> 
>     NULL,                                  /\* create location configuration \*/
>     NULL                                   /\* merge location configuration \*/
> };
> 
> 
> ngx\_module\_t  ngx\_http\_counter\_filter\_module = {
>     NGX\_MODULE\_V1,
>     &ngx\_http\_counter\_filter\_module\_ctx,   /\* module context \*/
>     NULL,                                  /\* module directives \*/
>     NGX\_HTTP\_MODULE,                       /\* module type \*/
>     NULL,                                  /\* init master \*/
>     NULL,                                  /\* init module \*/
>     NULL,                                  /\* init process \*/
>     NULL,                                  /\* init thread \*/
>     NULL,                                  /\* exit thread \*/
>     NULL,                                  /\* exit process \*/
>     NULL,                                  /\* exit master \*/
>     NGX\_MODULE\_V1\_PADDING
> };
> 
> 
> static ngx\_http\_output\_body\_filter\_pt  ngx\_http\_next\_body\_filter;
> 
> static ngx\_str\_t  ngx\_http\_counter\_name = ngx\_string("counter");
> 
> 
> static ngx\_int\_t
> ngx\_http\_counter\_body\_filter(ngx\_http\_request\_t \*r, ngx\_chain\_t \*in)
> {
>     ngx\_chain\_t                    \*cl;
>     ngx\_http\_counter\_filter\_ctx\_t  \*ctx;
> 
>     ctx = ngx\_http\_get\_module\_ctx(r, ngx\_http\_counter\_filter\_module);
>     if (ctx == NULL) {
>         ctx = ngx\_pcalloc(r->pool, sizeof(ngx\_http\_counter\_filter\_ctx\_t));
>         if (ctx == NULL) {
>             return NGX\_ERROR;
>         }
> 
>         ngx\_http\_set\_ctx(r, ctx, ngx\_http\_counter\_filter\_module);
>     }
> 
>     for (cl = in; cl; cl = cl->next) {
>         ctx->count += ngx\_buf\_size(cl->buf);
>     }
> 
>     return ngx\_http\_next\_body\_filter(r, in);
> }
> 
> 
> static ngx\_int\_t
> ngx\_http\_counter\_variable(ngx\_http\_request\_t \*r, ngx\_http\_variable\_value\_t \*v,
>     uintptr\_t data)
> {
>     u\_char                         \*p;
>     ngx\_http\_counter\_filter\_ctx\_t  \*ctx;
> 
>     ctx = ngx\_http\_get\_module\_ctx(r, ngx\_http\_counter\_filter\_module);
>     if (ctx == NULL) {
>         v->not\_found = 1;
>         return NGX\_OK;
>     }
> 
>     p = ngx\_pnalloc(r->pool, NGX\_OFF\_T\_LEN);
>     if (p == NULL) {
>         return NGX\_ERROR;
>     }
> 
>     v->data = p;
>     v->len = ngx\_sprintf(p, "%O", ctx->count) - p;
>     v->valid = 1;
>     v->no\_cacheable = 0;
>     v->not\_found = 0;
> 
>     return NGX\_OK;
> }
> 
> 
> static ngx\_int\_t
> ngx\_http\_counter\_add\_variables(ngx\_conf\_t \*cf)
> {
>     ngx\_http\_variable\_t  \*var;
> 
>     var = ngx\_http\_add\_variable(cf, &ngx\_http\_counter\_name, 0);
>     if (var == NULL) {
>         return NGX\_ERROR;
>     }
> 
>     var->get\_handler = ngx\_http\_counter\_variable;
> 
>     return NGX\_OK;
> }
> 
> 
> static ngx\_int\_t
> ngx\_http\_counter\_filter\_init(ngx\_conf\_t \*cf)
> {
>     ngx\_http\_next\_body\_filter = ngx\_http\_top\_body\_filter;
>     ngx\_http\_top\_body\_filter = ngx\_http\_counter\_body\_filter;
> 
>     return NGX\_OK;
> }

#### Building filter modules

When writing a body or header filter, pay special attention to the filter's position in the filter order. There's a number of header and body filters registered by nginx standard modules. The nginx standard modules register a number of head and body filters and it's important to register a new filter module in the right place with respect to them. Normally, modules register filters in their postconfiguration handlers. The order in which filters are called during processing is obviously the reverse of the order in which they are registered.

For third-party filter modules nginx provides a special slot `HTTP_AUX_FILTER_MODULES`. To register a filter module in this slot, set the `ngx_module_type` variable to `HTTP_AUX_FILTER` in the module's configuration.

The following example shows a filter module config file assuming for a module with just one source file, `ngx_http_foo_filter_module.c`.

> ngx\_module\_type=HTTP\_AUX\_FILTER
> ngx\_module\_name=ngx\_http\_foo\_filter\_module
> ngx\_module\_srcs="$ngx\_addon\_dir/ngx\_http\_foo\_filter\_module.c"
> 
> . auto/module

#### Buffer reuse

When issuing or altering a stream of buffers, it's often desirable to reuse the allocated buffers. A standard and widely adopted approach in nginx code is to keep two buffer chains for this purpose: `free` and `busy`. The `free` chain keeps all free buffers, which can be reused. The `busy` chain keeps all buffers sent by the current module that are still in use by some other filter handler. A buffer is considered in use if its size is greater than zero. Normally, when a buffer is consumed by a filter, its `pos` (or `file_pos` for a file buffer) is moved towards `last` (`file_last` for a file buffer). Once a buffer is completely consumed, it's ready to be reused. To add newly freed buffers to the `free` chain it's enough to iterate over the `busy` chain and move the zero size buffers at the head of it to `free`. This operation is so common that there is a special function for it, `ngx_chain_update_chains(free, busy, out, tag)`. The function appends the output chain `out` to `busy` and moves free buffers from the top of `busy` to `free`. Only the buffers with the specified `tag` are reused. This lets a module reuse only the buffers that it allocated itself.

The following example is a body filter that inserts the string “foo” before each incoming buffer. The new buffers allocated by the module are reused if possible. Note that for this example to work properly, setting up a [header filter](#http_header_filters) and resetting `content_length_n` to `-1` is also required, but the relevant code is not provided here.

> typedef struct {
>     ngx\_chain\_t  \*free;
>     ngx\_chain\_t  \*busy;
> }  ngx\_http\_foo\_filter\_ctx\_t;
> 
> 
> ngx\_int\_t
> ngx\_http\_foo\_body\_filter(ngx\_http\_request\_t \*r, ngx\_chain\_t \*in)
> {
>     ngx\_int\_t                   rc;
>     ngx\_buf\_t                  \*b;
>     ngx\_chain\_t                \*cl, \*tl, \*out, \*\*ll;
>     ngx\_http\_foo\_filter\_ctx\_t  \*ctx;
> 
>     ctx = ngx\_http\_get\_module\_ctx(r, ngx\_http\_foo\_filter\_module);
>     if (ctx == NULL) {
>         ctx = ngx\_pcalloc(r->pool, sizeof(ngx\_http\_foo\_filter\_ctx\_t));
>         if (ctx == NULL) {
>             return NGX\_ERROR;
>         }
> 
>         ngx\_http\_set\_ctx(r, ctx, ngx\_http\_foo\_filter\_module);
>     }
> 
>     /\* create a new chain "out" from "in" with all the changes \*/
> 
>     ll = &out;
> 
>     for (cl = in; cl; cl = cl->next) {
> 
>         /\* append "foo" in a reused buffer if possible \*/
> 
>         tl = ngx\_chain\_get\_free\_buf(r->pool, &ctx->free);
>         if (tl == NULL) {
>             return NGX\_ERROR;
>         }
> 
>         b = tl->buf;
>         b->tag = (ngx\_buf\_tag\_t) &ngx\_http\_foo\_filter\_module;
>         b->memory = 1;
>         b->pos = (u\_char \*) "foo";
>         b->last = b->pos + 3;
> 
>         \*ll = tl;
>         ll = &tl->next;
> 
>         /\* append the next incoming buffer \*/
> 
>         tl = ngx\_alloc\_chain\_link(r->pool);
>         if (tl == NULL) {
>             return NGX\_ERROR;
>         }
> 
>         tl->buf = cl->buf;
>         \*ll = tl;
>         ll = &tl->next;
>     }
> 
>     \*ll = NULL;
> 
>     /\* send the new chain \*/
> 
>     rc = ngx\_http\_next\_body\_filter(r, out);
> 
>     /\* update "busy" and "free" chains for reuse \*/
> 
>     ngx\_chain\_update\_chains(r->pool, &ctx->free, &ctx->busy, &out,
>                             (ngx\_buf\_tag\_t) &ngx\_http\_foo\_filter\_module);
> 
>     return rc;
> }

#### Load balancing

The [ngx\_http\_upstream\_module](../http/ngx_http_upstream_module.html) provides the basic functionality needed to pass requests to remote servers. Modules that implement specific protocols, such as HTTP or FastCGI, use this functionality. The module also provides an interface for creating custom load-balancing modules and implements a default round-robin method.

The [least\_conn](../http/ngx_http_upstream_module.html#least_conn) and [hash](../http/ngx_http_upstream_module.html#hash) modules implement alternative load-balancing methods, but are actually implemented as extensions of the upstream round-robin module and share a lot of code with it, such as the representation of a server group. The [keepalive](../http/ngx_http_upstream_module.html#keepalive) module is an independent module that extends upstream functionality.

The [ngx\_http\_upstream\_module](../http/ngx_http_upstream_module.html) can be configured explicitly by placing the corresponding [upstream](../http/ngx_http_upstream_module.html#upstream) block into the configuration file, or implicitly by using directives such as [proxy\_pass](../http/ngx_http_proxy_module.html#proxy_pass) that accept a URL that gets evaluated at some point into a list of servers. The alternative load-balancing methods are available only with an explicit upstream configuration. The upstream module configuration has its own directive context `NGX_HTTP_UPS_CONF`. The structure is defined as follows:

> struct ngx\_http\_upstream\_srv\_conf\_s {
>     ngx\_http\_upstream\_peer\_t         peer;
>     void                           \*\*srv\_conf;
> 
>     ngx\_array\_t                     \*servers;  /\* ngx\_http\_upstream\_server\_t \*/
> 
>     ngx\_uint\_t                       flags;
>     ngx\_str\_t                        host;
>     u\_char                          \*file\_name;
>     ngx\_uint\_t                       line;
>     in\_port\_t                        port;
>     ngx\_uint\_t                       no\_port;  /\* unsigned no\_port:1 \*/
> 
> #if (NGX\_HTTP\_UPSTREAM\_ZONE)
>     ngx\_shm\_zone\_t                  \*shm\_zone;
> #endif
> };

+   `srv_conf` — Configuration context of upstream modules.
+   `servers` — Array of `ngx_http_upstream_server_t`, the result of parsing a set of [server](../http/ngx_http_upstream_module.html#server) directives in the `upstream` block.
+   `flags` — Flags that mostly mark which features are supported by the load-balancing method. The features are configured as parameters of the [server](../http/ngx_http_upstream_module.html#server) directive:
    +   `NGX_HTTP_UPSTREAM_CREATE` — Distinguishes explicitly defined upstreams from those that are automatically created by the [proxy\_pass](../http/ngx_http_proxy_module.html#proxy_pass) directive and “friends” (FastCGI, SCGI, etc.)
    +   `NGX_HTTP_UPSTREAM_WEIGHT` — The “`weight`” parameter is supported
    +   `NGX_HTTP_UPSTREAM_MAX_FAILS` — The “`max_fails`” parameter is supported
    +   `NGX_HTTP_UPSTREAM_FAIL_TIMEOUT` — The “`fail_timeout`” parameter is supported
    +   `NGX_HTTP_UPSTREAM_DOWN` — The “`down`” parameter is supported
    +   `NGX_HTTP_UPSTREAM_BACKUP` — The “`backup`” parameter is supported
    +   `NGX_HTTP_UPSTREAM_MAX_CONNS` — The “`max_conns`” parameter is supported
+   `host` — Name of the upstream.
+   `file_name, line` — Name of the configuration file and the line where the `upstream` block is located.
+   `port` and `no_port` — Not used for explicitly defined upstream groups.
+   `shm_zone` — Shared memory zone used by this upstream group, if any.
+   `peer` — object that holds generic methods for initializing upstream configuration:
    
    > typedef struct {
    >     ngx\_http\_upstream\_init\_pt        init\_upstream;
    >     ngx\_http\_upstream\_init\_peer\_pt   init;
    >     void                            \*data;
    > } ngx\_http\_upstream\_peer\_t;
    
    A module that implements a load-balancing algorithm must set these methods and initialize private `data`. If `init_upstream` was not initialized during configuration parsing, `ngx_http_upstream_module` sets it to the default `ngx_http_upstream_init_round_robin` algorithm.
    +   `init_upstream(cf, us)` — Configuration-time method responsible for initializing a group of servers and initializing the `init()` method in case of success. A typical load-balancing module uses a list of servers in the `upstream` block to create an efficient data structure that it uses and saves its own configuration to the `data` field.
    +   `init(r, us)` — Initializes a per-request `ngx_http_upstream_peer_t.peer` structure that is used for load balancing (not to be confused with the `ngx_http_upstream_srv_conf_t.peer` described above which is per-upstream). It is passed as the `data` argument to all callbacks that deal with server selection.

When nginx has to pass a request to another host for processing, it uses the configured load-balancing method to obtain an address to connect to. The method is obtained from the `ngx_http_upstream_t.peer` object of type `ngx_peer_connection_t`:

> struct ngx\_peer\_connection\_s {
>     ...
> 
>     struct sockaddr                 \*sockaddr;
>     socklen\_t                        socklen;
>     ngx\_str\_t                       \*name;
> 
>     ngx\_uint\_t                       tries;
> 
>     ngx\_event\_get\_peer\_pt            get;
>     ngx\_event\_free\_peer\_pt           free;
>     ngx\_event\_notify\_peer\_pt         notify;
>     void                            \*data;
> 
> #if (NGX\_SSL || NGX\_COMPAT)
>     ngx\_event\_set\_peer\_session\_pt    set\_session;
>     ngx\_event\_save\_peer\_session\_pt   save\_session;
> #endif
> 
>     ...
> };

The structure has the following fields:

+   `sockaddr`, `socklen`, `name` — Address of the upstream server to connect to; this is the output parameter of a load-balancing method.
+   `data` — The per-request data of a load-balancing method; keeps the state of the selection algorithm and usually includes the link to the upstream configuration. It is passed as an argument to all methods that deal with server selection (see [below](#lb_method_get)).
+   `tries` — Allowed [number](../http/ngx_http_proxy_module.html#proxy_next_upstream_tries) of attempts to connect to an upstream server.
+   `get`, `free`, `notify`, `set_session`, and `save_session` - Methods of the load-balancing module, described below.

All methods accept at least two arguments: a peer connection object `pc` and the `data` created by `ngx_http_upstream_srv_conf_t.peer.init()`. Note that it might differ from `pc.data` due to “chaining” of load-balancing modules.

+   `get(pc, data)` — The method called when the upstream module is ready to pass a request to an upstream server and needs to know its address. The method has to fill the `sockaddr`, `socklen`, and `name` fields of `ngx_peer_connection_t` structure. The return is one of:
    +   `NGX_OK` — Server was selected.
    +   `NGX_ERROR` — Internal error occurred.
    +   `NGX_BUSY` — no servers are currently available. This can happen due to many reasons, including: the dynamic server group is empty, all servers in the group are in the failed state, or all servers in the group are already handling the maximum number of connections.
    +   `NGX_DONE` — the underlying connection was reused and there is no need to create a new connection to the upstream server. This value is set by the `keepalive` module.
+   `free(pc, data, state)` — The method called when an upstream module has finished work with a particular server. The `state` argument is the completion status of the upstream connection, a bitmask with the following possible values:
    
    +   `NGX_PEER_FAILED` — Attempt was [unsuccessful](../http/ngx_http_upstream_module.html#max_fails)
    +   `NGX_PEER_NEXT` — A special case when upstream server returns codes `403` or `404`, which are not considered a [failure](../http/ngx_http_upstream_module.html#max_fails).
    +   `NGX_PEER_KEEPALIVE` — Currently unused
    
    This method also decrements the `tries` counter.
+   `notify(pc, data, type)` — Currently unused in the OSS version.
+   `set_session(pc, data)` and `save_session(pc, data)` — SSL-specific methods that enable caching sessions to upstream servers. The implementation is provided by the round-robin balancing method.

#### Examples

The [nginx-dev-examples](https://github.com/nginx/nginx-dev-examples) repository provides nginx module examples.

#### Code style

#### General rules

+   maximum text width is 80 characters
+   indentation is 4 spaces
+   no tabs, no trailing spaces
+   list elements on the same line are separated with spaces
+   hexadecimal literals are lowercase
+   file names, function and type names, and global variables have the `ngx_` or more specific prefix such as `ngx_http_` and `ngx_mail_`

> size\_t
> ngx\_utf8\_length(u\_char \*p, size\_t n)
> {
>     u\_char  c, \*last;
>     size\_t  len;
> 
>     last = p + n;
> 
>     for (len = 0; p < last; len++) {
> 
>         c = \*p;
> 
>         if (c < 0x80) {
>             p++;
>             continue;
>         }
> 
>         if (ngx\_utf8\_decode(&p, last - p) > 0x10ffff) {
>             /\* invalid UTF-8 \*/
>             return n;
>         }
>     }
> 
>     return len;
> }

#### Files

A typical source file may contain the following sections separated by two empty lines:

+   copyright statements
+   includes
+   preprocessor definitions
+   type definitions
+   function prototypes
+   variable definitions
+   function definitions

Copyright statements look like this:

> /\*
>  \* Copyright (C) Author Name
>  \* Copyright (C) Organization, Inc.
>  \*/

If the file is modified significantly, the list of authors should be updated, the new author is added to the top.

The `ngx_config.h` and `ngx_core.h` files are always included first, followed by one of `ngx_http.h`, `ngx_stream.h`, or `ngx_mail.h`. Then follow optional external header files:

> #include <ngx\_config.h>
> #include <ngx\_core.h>
> #include <ngx\_http.h>
> 
> #include <libxml/parser.h>
> #include <libxml/tree.h>
> #include <libxslt/xslt.h>
> 
> #if (NGX\_HAVE\_EXSLT)
> #include <libexslt/exslt.h>
> #endif

Header files should include the so called "header protection":

> #ifndef \_NGX\_PROCESS\_CYCLE\_H\_INCLUDED\_
> #define \_NGX\_PROCESS\_CYCLE\_H\_INCLUDED\_
> ...
> #endif /\* \_NGX\_PROCESS\_CYCLE\_H\_INCLUDED\_ \*/

#### Comments

+   “`//`” comments are not used
+   text is written in English, American spelling is preferred
+   multi-line comments are formatted like this:
    
    > /\*
    >  \* The red-black tree code is based on the algorithm described in
    >  \* the "Introduction to Algorithms" by Cormen, Leiserson and Rivest.
    >  \*/
    
    > /\* find the server configuration for the address:port \*/
    

#### Preprocessor

Macro names start from `ngx_` or `NGX_` (or more specific) prefix. Macro names for constants are uppercase. Parameterized macros and macros for initializers are lowercase. The macro name and value are separated by at least two spaces:

> #define NGX\_CONF\_BUFFER  4096
> 
> #define ngx\_buf\_in\_memory(b)  (b->temporary || b->memory || b->mmap)
> 
> #define ngx\_buf\_size(b)                                                      \\
>     (ngx\_buf\_in\_memory(b) ? (off\_t) (b->last - b->pos):                      \\
>                             (b->file\_last - b->file\_pos))
> 
> #define ngx\_null\_string  { 0, NULL }

Conditions are inside parentheses, negation is outside:

> #if (NGX\_HAVE\_KQUEUE)
> ...
> #elif ((NGX\_HAVE\_DEVPOLL && !(NGX\_TEST\_BUILD\_DEVPOLL)) \\
>        || (NGX\_HAVE\_EVENTPORT && !(NGX\_TEST\_BUILD\_EVENTPORT)))
> ...
> #elif (NGX\_HAVE\_EPOLL && !(NGX\_TEST\_BUILD\_EPOLL))
> ...
> #elif (NGX\_HAVE\_POLL)
> ...
> #else /\* select \*/
> ...
> #endif /\* NGX\_HAVE\_KQUEUE \*/

#### Types

Type names end with the “`_t`” suffix. A defined type name is separated by at least two spaces:

> typedef ngx\_uint\_t  ngx\_rbtree\_key\_t;

Structure types are defined using `typedef`. Inside structures, member types and names are aligned:

> typedef struct {
>     size\_t      len;
>     u\_char     \*data;
> } ngx\_str\_t;

Keep alignment identical among different structures in the file. A structure that points to itself has the name, ending with “`_s`”. Adjacent structure definitions are separated with two empty lines:

> typedef struct ngx\_list\_part\_s  ngx\_list\_part\_t;
> 
> struct ngx\_list\_part\_s {
>     void             \*elts;
>     ngx\_uint\_t        nelts;
>     ngx\_list\_part\_t  \*next;
> };
> 
> 
> typedef struct {
>     ngx\_list\_part\_t  \*last;
>     ngx\_list\_part\_t   part;
>     size\_t            size;
>     ngx\_uint\_t        nalloc;
>     ngx\_pool\_t       \*pool;
> } ngx\_list\_t;

Each structure member is declared on its own line:

> typedef struct {
>     ngx\_uint\_t        hash;
>     ngx\_str\_t         key;
>     ngx\_str\_t         value;
>     u\_char           \*lowcase\_key;
> } ngx\_table\_elt\_t;

Function pointers inside structures have defined types ending with “`_pt`”:

> typedef ssize\_t (\*ngx\_recv\_pt)(ngx\_connection\_t \*c, u\_char \*buf, size\_t size);
> typedef ssize\_t (\*ngx\_recv\_chain\_pt)(ngx\_connection\_t \*c, ngx\_chain\_t \*in,
>     off\_t limit);
> typedef ssize\_t (\*ngx\_send\_pt)(ngx\_connection\_t \*c, u\_char \*buf, size\_t size);
> typedef ngx\_chain\_t \*(\*ngx\_send\_chain\_pt)(ngx\_connection\_t \*c, ngx\_chain\_t \*in,
>     off\_t limit);
> 
> typedef struct {
>     ngx\_recv\_pt        recv;
>     ngx\_recv\_chain\_pt  recv\_chain;
>     ngx\_recv\_pt        udp\_recv;
>     ngx\_send\_pt        send;
>     ngx\_send\_pt        udp\_send;
>     ngx\_send\_chain\_pt  udp\_send\_chain;
>     ngx\_send\_chain\_pt  send\_chain;
>     ngx\_uint\_t         flags;
> } ngx\_os\_io\_t;

Enumerations have types ending with “`_e`”:

> typedef enum {
>     ngx\_http\_fastcgi\_st\_version = 0,
>     ngx\_http\_fastcgi\_st\_type,
>     ...
>     ngx\_http\_fastcgi\_st\_padding
> } ngx\_http\_fastcgi\_state\_e;

#### Variables

Variables are declared sorted by length of a base type, then alphabetically. Type names and variable names are aligned. The type and name “columns” are separated with two spaces. Large arrays are put at the end of a declaration block:

> u\_char                      |  | \*rv, \*p;
> ngx\_conf\_t                  |  | \*cf;
> ngx\_uint\_t                  |  |  i, j, k;
> unsigned int                |  |  len;
> struct sockaddr             |  | \*sa;
> const unsigned char         |  | \*data;
> ngx\_peer\_connection\_t       |  | \*pc;
> ngx\_http\_core\_srv\_conf\_t    |  |\*\*cscfp;
> ngx\_http\_upstream\_srv\_conf\_t|  | \*us, \*uscf;
> u\_char                      |  |  text\[NGX\_SOCKADDR\_STRLEN\];

Static and global variables may be initialized on declaration:

> static ngx\_str\_t  ngx\_http\_memcached\_key = ngx\_string("memcached\_key");

> static ngx\_uint\_t  mday\[\] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

> static uint32\_t  ngx\_crc32\_table16\[\] = {
>     0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
>     ...
>     0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
> };

There is a bunch of commonly used type/name combinations:

> u\_char                        \*rv;
> ngx\_int\_t                      rc;
> ngx\_conf\_t                    \*cf;
> ngx\_connection\_t              \*c;
> ngx\_http\_request\_t            \*r;
> ngx\_peer\_connection\_t         \*pc;
> ngx\_http\_upstream\_srv\_conf\_t  \*us, \*uscf;

#### Functions

All functions (even static ones) should have prototypes. Prototypes include argument names. Long prototypes are wrapped with a single indentation on continuation lines:

> static char \*ngx\_http\_block(ngx\_conf\_t \*cf, ngx\_command\_t \*cmd, void \*conf);
> static ngx\_int\_t ngx\_http\_init\_phases(ngx\_conf\_t \*cf,
>     ngx\_http\_core\_main\_conf\_t \*cmcf);
> 
> static char \*ngx\_http\_merge\_servers(ngx\_conf\_t \*cf,
>     ngx\_http\_core\_main\_conf\_t \*cmcf, ngx\_http\_module\_t \*module,
>     ngx\_uint\_t ctx\_index);

The function name in a definition starts with a new line. The function body opening and closing braces are on separate lines. The body of a function is indented. There are two empty lines between functions:

> static ngx\_int\_t
> ngx\_http\_find\_virtual\_server(ngx\_http\_request\_t \*r, u\_char \*host, size\_t len)
> {
>     ...
> }
> 
> 
> static ngx\_int\_t
> ngx\_http\_add\_addresses(ngx\_conf\_t \*cf, ngx\_http\_core\_srv\_conf\_t \*cscf,
>     ngx\_http\_conf\_port\_t \*port, ngx\_http\_listen\_opt\_t \*lsopt)
> {
>     ...
> }

There is no space after the function name and opening parenthesis. Long function calls are wrapped such that continuation lines start from the position of the first function argument. If this is impossible, format the first continuation line such that it ends at position 79:

> ngx\_log\_debug2(NGX\_LOG\_DEBUG\_HTTP, r->connection->log, 0,
>                "http header: \\"%V: %V\\"",
>                &h->key, &h->value);
> 
> hc->busy = ngx\_palloc(r->connection->pool,
>                   cscf->large\_client\_header\_buffers.num \* sizeof(ngx\_buf\_t \*));

The `ngx_inline` macro should be used instead of `inline`:

> static ngx\_inline void ngx\_cpuid(uint32\_t i, uint32\_t \*buf);

#### Expressions

Binary operators except “`.`” and “`−>`” should be separated from their operands by one space. Unary operators and subscripts are not separated from their operands by spaces:

> width = width \* 10 + (\*fmt++ - '0');

> ch = (u\_char) ((decoded << 4) + (ch - '0'));

> r->exten.data = &r->uri.data\[i + 1\];

Type casts are separated by one space from casted expressions. An asterisk inside type cast is separated with space from type name:

> len = ngx\_sock\_ntop((struct sockaddr \*) sin6, p, len, 1);

If an expression does not fit into single line, it is wrapped. The preferred point to break a line is a binary operator. The continuation line is lined up with the start of expression:

> if (status == NGX\_HTTP\_MOVED\_PERMANENTLY
>     || status == NGX\_HTTP\_MOVED\_TEMPORARILY
>     || status == NGX\_HTTP\_SEE\_OTHER
>     || status == NGX\_HTTP\_TEMPORARY\_REDIRECT
>     || status == NGX\_HTTP\_PERMANENT\_REDIRECT)
> {
>     ...
> }

> p->temp\_file->warn = "an upstream response is buffered "
>                      "to a temporary file";

As a last resort, it is possible to wrap an expression so that the continuation line ends at position 79:

> hinit->hash = ngx\_pcalloc(hinit->pool, sizeof(ngx\_hash\_wildcard\_t)
>                                      + size \* sizeof(ngx\_hash\_elt\_t \*));

The above rules also apply to sub-expressions, where each sub-expression has its own indentation level:

> if (((u->conf->cache\_use\_stale & NGX\_HTTP\_UPSTREAM\_FT\_UPDATING)
>      || c->stale\_updating) && !r->background
>     && u->conf->cache\_background\_update)
> {
>     ...
> }

Sometimes, it is convenient to wrap an expression after a cast. In this case, the continuation line is indented:

> node = (ngx\_rbtree\_node\_t \*)
>            ((u\_char \*) lr - offsetof(ngx\_rbtree\_node\_t, color));

Pointers are explicitly compared to `NULL` (not `0`):

> if (ptr != NULL) {
>     ...
> }

#### Conditionals and Loops

The “`if`” keyword is separated from the condition by one space. Opening brace is located on the same line, or on a dedicated line if the condition takes several lines. Closing brace is located on a dedicated line, optionally followed by “`else if` / `else`”. Usually, there is an empty line before the “`else if` / `else`” part:

> if (node->left == sentinel) {
>     temp = node->right;
>     subst = node;
> 
> } else if (node->right == sentinel) {
>     temp = node->left;
>     subst = node;
> 
> } else {
>     subst = ngx\_rbtree\_min(node->right, sentinel);
> 
>     if (subst->left != sentinel) {
>         temp = subst->left;
> 
>     } else {
>         temp = subst->right;
>     }
> }

Similar formatting rules are applied to “`do`” and “`while`” loops:

> while (p < last && \*p == ' ') {
>     p++;
> }

> do {
>     ctx->node = rn;
>     ctx = ctx->next;
> } while (ctx);

The “`switch`” keyword is separated from the condition by one space. Opening brace is located on the same line. Closing brace is located on a dedicated line. The “`case`” keywords are lined up with “`switch`”:

> switch (ch) {
> case '!':
>     looked = 2;
>     state = ssi\_comment0\_state;
>     break;
> 
> case '<':
>     copy\_end = p;
>     break;
> 
> default:
>     copy\_end = p;
>     looked = 0;
>     state = ssi\_start\_state;
>     break;
> }

Most “`for`” loops are formatted like this:

> for (i = 0; i < ccf->env.nelts; i++) {
>     ...
> }

> for (q = ngx\_queue\_head(locations);
>      q != ngx\_queue\_sentinel(locations);
>      q = ngx\_queue\_next(q))
> {
>     ...
> }

If some part of the “`for`” statement is omitted, this is indicated by the “`/* void */`” comment:

> for (i = 0; /\* void \*/ ; i++) {
>     ...
> }

A loop with an empty body is also indicated by the “`/* void */`” comment which may be put on the same line:

> for (cl = \*busy; cl->next; cl = cl->next) { /\* void \*/ }

An endless loop looks like this:

> for ( ;; ) {
>     ...
> }

#### Labels

Labels are surrounded with empty lines and are indented at the previous level:

>     if (i == 0) {
>         u->err = "host not found";
>         goto failed;
>     }
> 
>     u->addrs = ngx\_pcalloc(pool, i \* sizeof(ngx\_addr\_t));
>     if (u->addrs == NULL) {
>         goto failed;
>     }
> 
>     u->naddrs = i;
> 
>     ...
> 
>     return NGX\_OK;
> 
> failed:
> 
>     freeaddrinfo(res);
>     return NGX\_ERROR;

#### Debugging memory issues

To debug memory issues such as buffer overruns or use-after-free errors, you can use the [AddressSanitizer](https://en.wikipedia.org/wiki/AddressSanitizer) (ASan) supported by some modern compilers. To enable ASan with `gcc` and `clang`, use the `-fsanitize=address` compiler and linker option. When building nginx, this can be done by adding the option to `--with-cc-opt` and `--with-ld-opt` parameters of the `configure` script.

Since most allocations in nginx are made from nginx internal [pool](#pool), enabling ASan may not always be enough to debug memory issues. The internal pool allocates a big chunk of memory from the system and cuts smaller allocations from it. However, this mechanism can be disabled by setting the `NGX_DEBUG_PALLOC` macro to `1`. In this case, allocations are passed directly to the system allocator giving it full control over the buffers boundaries.

The following configuration line summarizes the information provided above. It is recommended while developing third-party modules and testing nginx on different platforms.

> auto/configure --with-cc-opt='-fsanitize=address -DNGX\_DEBUG\_PALLOC=1'
>                --with-ld-opt=-fsanitize=address

#### Common Pitfalls

#### Writing a C module

The most common pitfall is an attempt to write a full-fledged C module when it can be avoided. In most cases your task can be accomplished by creating a proper configuration. If writing a module is inevitable, try to make it as small and simple as possible. For example, a module can only export some [variables](#http_variables).

Before starting a module, consider the following questions:

+   Is it possible to implement a desired feature using already [available modules](../../docs/index.html)?
+   Is it possible to solve an issue using built-in scripting languages, such as [Perl](../http/ngx_http_perl_module.html) or [njs](../njs/index.html)?

#### C Strings

The most used string type in nginx, [ngx\_str\_t](#string_overview) is not a C-Style zero-terminated string. You cannot pass the data to standard C library functions such as `strlen()` or `strstr()`. Instead, nginx [counterparts](#string_overview) that accept either `ngx_str_t` should be used or pointer to data and a length. However, there is a case when `ngx_str_t` holds a pointer to a zero-terminated string: strings that come as a result of configuration file parsing are zero-terminated.

#### Global Variables

Avoid using global variables in your modules. Most likely this is an error to have a global variable. Any global data should be tied to a [configuration cycle](#cycle) and be allocated from the corresponding [memory pool](#pool). This allows nginx to perform graceful configuration reloads. An attempt to use global variables will likely break this feature, because it will be impossible to have two configurations at the same time and get rid of them. Sometimes global variables are required. In this case, special attention is needed to manage reconfiguration properly. Also, check if libraries used by your code have implicit global state that may be broken on reload.

#### Manual Memory Management

Instead of dealing with malloc/free approach which is error prone, learn how to use nginx [pools](#pool). A pool is created and tied to an object - [configuration](#http_conf), [cycle](#cycle), [connection](#connection), or [HTTP request](#http_request). When the object is destroyed, the associated pool is destroyed too. So when working with an object, it is possible to allocate the amount needed from the corresponding pool and don't care about freeing memory even in case of errors.

#### Threads

It is recommended to avoid using threads in nginx because it will definitely break things: most nginx functions are not thread-safe. It is expected that a thread will be executing only system calls and thread-safe library functions. If you need to run some code that is not related to client request processing, the proper way is to schedule a timer in the `init_process` module handler and perform required actions in timer handler. Internally nginx makes use of [threads](#threads) to boost IO-related operations, but this is a special case with a lot of limitations.

#### Blocking Libraries

A common mistake is to use libraries that are blocking internally. Most libraries out there are synchronous and blocking by nature. In other words, they perform one operation at a time and waste time waiting for response from other peer. As a result, when a request is processed with such library, whole nginx worker is blocked, thus destroying performance. Use only libraries that provide asynchronous interface and don't block whole process.

#### HTTP Requests to External Services

Often modules need to perform an HTTP call to some external service. A common mistake is to use some external library, such as libcurl, to perform the HTTP request. It is absolutely unnecessary to bring a huge amount of external (probably [blocking](#using_libraries)!) code for the task which can be accomplished by nginx itself.

There are two basic usage scenarios when an external request is needed:

+   in the context of processing a client request (for example, in content handler)
+   in the context of a worker process (for example, timer handler)

In the first case, the best is to use [subrequests API](#http_subrequests). Instead of directly accessing external service, you declare a location in nginx configuration and direct your subrequest to this location. This location is not limited to [proxying](../http/ngx_http_proxy_module.html#proxy_pass) requests, but may contain other nginx directives. An example of such approach is the [auth\_request](../http/ngx_http_auth_request_module.html#auth_request) directive implemented in [ngx\_http\_auth\_request module](https://github.com/nginx/nginx/blob/master/src/http/modules/ngx_http_auth_request_module.c).

For the second case, it is possible to use basic HTTP client functionality available in nginx. For example, [OCSP module](https://github.com/nginx/nginx/blob/master/src/event/ngx_event_openssl_stapling.c) implements simple HTTP client.