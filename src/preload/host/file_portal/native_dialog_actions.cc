// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Overrides gtk_widget_* and gtk_window_* functions to work on our hidden GtkNativeDialog.

#include <gtk/gtk.h>

#include "base/base.h"
#include "preload/declare_override.h"

DECLARE_OVERRIDE_THROW(void, gtk_widget_show_all, GtkWidget* widget) {
  if (GTK_IS_NATIVE_DIALOG(widget)) {
    gtk_native_dialog_show(reinterpret_cast<GtkNativeDialog*>(widget));
    return;
  }

  auto original = LoadOriginal();
  original(widget);
}

DECLARE_OVERRIDE_THROW(void, gtk_widget_hide, GtkWidget* widget) {
  if (GTK_IS_NATIVE_DIALOG(widget)) {
    gtk_native_dialog_hide(reinterpret_cast<GtkNativeDialog*>(widget));
    return;
  }

  auto original = LoadOriginal();
  original(widget);
}

DECLARE_OVERRIDE_THROW(gboolean, gtk_widget_hide_on_delete, GtkWidget* widget) {
  if (GTK_IS_NATIVE_DIALOG(widget)) {
    gtk_native_dialog_hide(reinterpret_cast<GtkNativeDialog*>(widget));
    return TRUE;
  }

  auto original = LoadOriginal();
  return original(widget);
}

DECLARE_OVERRIDE_THROW(void, gtk_widget_destroy, GtkWidget* widget) {
  if (GTK_IS_NATIVE_DIALOG(widget)) {
    gtk_native_dialog_destroy(reinterpret_cast<GtkNativeDialog*>(widget));
    return;
  }

  auto original = LoadOriginal();
  original(widget);
}

DECLARE_OVERRIDE_THROW(void, gtk_window_set_modal, GtkWindow* window, gboolean modal) {
  if (GTK_IS_NATIVE_DIALOG(window)) {
    // Aura needs the XID which the client never knows, so the easiest thing to do is just...don't
    // allow transient dialogs.
    fprintf(stderr, "ZYPAK ERROR: gtk_window_set_modal called on GtkNativeDialog\n");
    abort();
  }

  auto original = LoadOriginal();
  return original(window, modal);
}

DECLARE_OVERRIDE_THROW(void, gtk_window_present_with_time, GtkWindow* window, guint32 time) {
  if (GTK_IS_NATIVE_DIALOG(window)) {
    return;
  }

  auto original = LoadOriginal();
  return original(window, time);
}
