// Copyright 2020 Endless Mobile, Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Ensures GTK_WINDOW(...) casts pass on our hidden GtkNativeDialog.

#include <gtk/gtk.h>

#include "base/base.h"
#include "preload/declare_override.h"

DECLARE_OVERRIDE_THROW(GTypeInstance*, g_type_check_instance_cast, GTypeInstance* type_instance,
                       GType iface_type) {
  if (iface_type == GTK_TYPE_WINDOW && GTK_IS_NATIVE_DIALOG(type_instance)) {
    return type_instance;
  }

  auto original = LoadOriginal();
  return original(type_instance, iface_type);
}
