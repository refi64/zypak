// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Makes gtk_file_chooser_dialog_new return a GtkFileChooserNative, stored inside a GtkWidget*.

#include <cstdarg>
#include <cstdio>
#include <unordered_map>
#include <vector>

#include <gtk/gtk.h>

#include "base/base.h"
#include "base/debug.h"
#include "preload/declare_override.h"

DECLARE_OVERRIDE_THROW(GtkWidget*, gtk_file_chooser_dialog_new, const char* title,
                       GtkWindow* parent, GtkFileChooserAction action,
                       const char* first_button_text, ...) {
  // File portal has no cancel label, so we ignore that and only handle accept.
  const char* accept_label = nullptr;

  va_list va;
  va_start(va, first_button_text);

  const char* label = first_button_text;
  do {
    int response = va_arg(va, int);
    if (response == GTK_RESPONSE_ACCEPT) {
      accept_label = label;
    }
  } while ((label = va_arg(va, const char*)) != nullptr);

  va_end(va);

  if (accept_label == nullptr) {
    fputs("Cannot find accept button for file chooser\n", stderr);
    abort();
  }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  // Stock icons don't work with the portal. Therefore, if that was being used, just drop it
  // entirely and let it use the defaults (which is the semantic meaning of using stock icons
  // anyway).
  if (strcmp(accept_label, GTK_STOCK_OPEN) == 0) {
    accept_label = nullptr;
  }
#pragma clang diagnostic pop

  GtkFileChooserNative* native =
      gtk_file_chooser_native_new(title, parent, action, accept_label, nullptr);
  return reinterpret_cast<GtkWidget*>(native);
}
