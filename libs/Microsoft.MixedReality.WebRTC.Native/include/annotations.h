// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

// The following is borrowed from the WebRTC codebase, with renamed prefix MRS_.

//
// Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
//
// Borrowed from
// https://code.google.com/p/gperftools/source/browse/src/base/thread_annotations.h
// but adapted for clang attributes instead of the gcc.
//
// This header file contains the macro definitions for thread safety
// annotations that allow the developers to document the locking policies
// of their multi-threaded code. The annotations can also help program
// analysis tools to identify potential thread safety issues.

#if defined(__clang__) && (!defined(SWIG))
#define MRS_THREAD_ANNOTATION_ATTRIBUTE__(x) __attribute__((x))
#else
#define MRS_THREAD_ANNOTATION_ATTRIBUTE__(x)  // no-op
#endif

// Document if a shared variable/field needs to be protected by a lock.
// GUARDED_BY allows the user to specify a particular lock that should be
// held when accessing the annotated variable.
#define MRS_GUARDED_BY(x) MRS_THREAD_ANNOTATION_ATTRIBUTE__(guarded_by(x))

// Document if the memory location pointed to by a pointer should be guarded
// by a lock when dereferencing the pointer. Note that a pointer variable to a
// shared memory location could itself be a shared variable. For example, if a
// shared global pointer q, which is guarded by mu1, points to a shared memory
// location that is guarded by mu2, q should be annotated as follows:
//     int *q GUARDED_BY(mu1) PT_GUARDED_BY(mu2);
#define MRS_PT_GUARDED_BY(x) MRS_THREAD_ANNOTATION_ATTRIBUTE__(pt_guarded_by(x))
