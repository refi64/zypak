define build_objects_inner

$(1)_SOURCE_DIR ?= $(1)
$(1)_SOURCE_DIR_FULL := src/$$($(1)_SOURCE_DIR)
$(1)_SOURCE_PATHS := $$(patsubst %,$$($(1)_SOURCE_DIR_FULL)/%,$$($(1)_SOURCES))
$(1)_OBJECTS := $$(patsubst %.cc,$(OBJ)/$(1)/%.o,$$($(1)_SOURCES))

_$(1)_builddir:
	@mkdir -p $$(dir $$($(1)_OBJECTS))

$(OBJ)/$(1)/%.o $(DEP)/$(1)/%.d: $$($(1)_SOURCE_DIR_FULL)/%.cc | _$(1)_builddir
	$(CXX) $(CXXFLAGS) $$($(1)_CXXFLAGS) -MD -c -o $$@ $$<

-include $$($(1)_OBJECTS:.o=.d)

endef

define build_linked_inner

$(1)_DEP_FILES := $$(foreach dep,$$($(1)_DEPS),$$($$(dep)_OUTPUT))
$(1)_LIBS += $$(foreach dep,$$($(1)_DEPS),$$($$(dep)_PUBLIC_LIBS))

$$($(1)_OUTPUT): $$($(1)_OBJECTS) $$($(1)_DEP_FILES)
	$(CXX) $(CXXFLAGS) $$($(1)_LDFLAGS) -o $$@ $$^ $$($(1)_LIBS)

all : $$($(1)_OUTPUT)

endef

define build_shlib_inner

$(call build_objects_inner,$(1))

$(1)_NAME ?= $(1)
$(1)_OUTPUT := $(BUILD)/lib$$($(1)_NAME).so
$(1)_CXXFLAGS += -fPIC
$(1)_LDFLAGS += -shared

$(call build_linked_inner,$(1))

endef

build_shlib = $(eval $(call build_shlib_inner,$(1)))

define build_stlib_inner

$(call build_objects_inner,$(1))

$(1)_NAME ?= $(1)
$(1)_OUTPUT := $(BUILD)/lib$$($(1)_NAME).a

$$($(1)_OUTPUT): $$($(1)_OBJECTS)
	@rm -f $$@
	ar rcs $$@ $$^

endef

build_stlib = $(eval $(call build_stlib_inner,$(1)))

define build_exe_inner

$(call build_objects_inner,$(1))

$(1)_NAME ?= $(1)
$(1)_OUTPUT := $(BUILD)/$$($(1)_NAME)

$(call build_linked_inner,$(1))

endef

build_exe = $(eval $(call build_exe_inner,$(1)))
