// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Makes gtk_file_chooser_dialog_new return a GtkFileChooserNative, stored inside a GtkWidget*.

#include <cstdarg>
#include <unordered_map>
#include <vector>

#include <gtk/gtk.h>

#include "base/base.h"
#include "base/debug.h"
#include "preload/declare_override.h"

DECLARE_OVERRIDE_THROW(GtkWidget*, gtk_file_chooser_dialog_new, const char* title,
                       GtkWindow* parent, GtkFileChooserAction action,
                       const char* first_button_text, ...) {
  std::unordered_map<int, const char*> buttons;

  va_list va;
  va_start(va, first_button_text);

  const char* label = first_button_text;
  do {
    int response = va_arg(va, int);
    buttons[response] = label;
  } while ((label = va_arg(va, const char*)) != nullptr);

  va_end(va);

  if (buttons.size() != 2 || buttons.find(GTK_RESPONSE_ACCEPT) == buttons.end() ||
      buttons.find(GTK_RESPONSE_CANCEL) == buttons.end()) {
    fprintf(stderr,
            "Unexpected buttons in gtk_file_chooser_dialog_new "
            "(length: %zu, accept: %s, cancel: %s)\n",
            buttons.size(), buttons[GTK_RESPONSE_ACCEPT], buttons[GTK_RESPONSE_CANCEL]);
    abort();
  }

  GtkFileChooserNative* native = gtk_file_chooser_native_new(
      title, parent, action, buttons[GTK_RESPONSE_ACCEPT], buttons[GTK_RESPONSE_CANCEL]);
  return reinterpret_cast<GtkWidget*>(native);
}
