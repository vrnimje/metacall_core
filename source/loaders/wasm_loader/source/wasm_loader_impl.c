/*
 *	Loader Library by Parra Studios
 *	A plugin for loading WebAssembly code at run-time into a process.
 *
 *	Copyright (C) 2016 - 2021 Vicente Eduardo Ferrer Garcia <vic798@gmail.com>
 *
 *	Licensed under the Apache License, Version 2.0 (the "License");
 *	you may not use this file except in compliance with the License.
 *	You may obtain a copy of the License at
 *
 *		http://www.apache.org/licenses/LICENSE-2.0
 *
 *	Unless required by applicable law or agreed to in writing, software
 *	distributed under the License is distributed on an "AS IS" BASIS,
 *	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *	See the License for the specific language governing permissions and
 *	limitations under the License.
 *
 */

#include <wasm_loader/wasm_loader_impl.h>

#include <loader/loader.h>
#include <loader/loader_impl.h>

#include <reflect/reflect_context.h>
#include <reflect/reflect_function.h>
#include <reflect/reflect_scope.h>
#include <reflect/reflect_type.h>

#include <log/log.h>

#include <adt/adt_vector.h>

#include <stdlib.h>

#include <wasm.h>

#define COUNT_OF(array) (sizeof(array) / sizeof(array[0]))

typedef struct loader_impl_wasm_function_type
{
	const wasm_func_t *func;
} * loader_impl_wasm_function;

typedef struct loader_impl_wasm_module_type
{
	wasm_module_t *module;
	wasm_instance_t *instance;
	wasm_extern_vec_t exports;
} loader_impl_wasm_module;

typedef struct loader_impl_wasm_handle_type
{
	vector modules;
} * loader_impl_wasm_handle;

typedef struct loader_impl_wasm_type
{
	wasm_engine_t *engine;
	wasm_store_t *store;
	vector paths;
} * loader_impl_wasm;

int type_wasm_interface_create(type t, type_impl impl)
{
	/* TODO */

	(void)t;
	(void)impl;

	return 0;
}

void type_wasm_interface_destroy(type t, type_impl impl)
{
	/* TODO */

	(void)t;
	(void)impl;
}

type_interface type_wasm_singleton(void)
{
	static struct type_interface_type wasm_type_interface = {
		&type_wasm_interface_create,
		&type_wasm_interface_destroy
	};

	return &wasm_type_interface;
}

int function_wasm_interface_create(function func, function_impl impl)
{
	/* TODO */

	(void)func;
	(void)impl;

	return 0;
}

function_return function_wasm_interface_invoke(function func, function_impl impl, function_args args, size_t size)
{
	/* TODO */

	(void)func;
	(void)impl;
	(void)args;
	(void)size;

	return NULL;
}

function_return function_wasm_interface_await(function func, function_impl impl, function_args args, size_t size, function_resolve_callback resolve_callback, function_reject_callback reject_callback, void *context)
{
	/* TODO */

	(void)func;
	(void)impl;
	(void)args;
	(void)size;
	(void)resolve_callback;
	(void)reject_callback;
	(void)context;

	return NULL;
}

void function_wasm_interface_destroy(function func, function_impl impl)
{
	(void)func;

	free((loader_impl_wasm_function)impl);
}

function_interface function_wasm_singleton(void)
{
	static struct function_interface_type wasm_function_interface = {
		&function_wasm_interface_create,
		&function_wasm_interface_invoke,
		&function_wasm_interface_await,
		&function_wasm_interface_destroy
	};

	return &wasm_function_interface;
}

int wasm_loader_impl_initialize_types(loader_impl impl)
{
	// TODO: Implement ANYREF and FUNCREF?
	static struct
	{
		type_id id;
		const char *name;
	} type_names[] = {
		{ TYPE_INT, "i32" },
		{ TYPE_LONG, "i64" },
		{ TYPE_FLOAT, "f32" },
		{ TYPE_DOUBLE, "f64" },
	};

	const size_t size = COUNT_OF(type_names);

	for (size_t i = 0; i < size; i++)
	{
		// TODO: Do we need error handling here?
		type t = type_create(type_names[i].id, type_names[i].name, NULL, NULL);
		loader_impl_type_define(impl, type_name(t), t);
	}

	return 0;
}

loader_impl_data wasm_loader_impl_initialize(loader_impl impl, configuration config)
{
	(void)config;

	loader_impl_wasm wasm_impl = malloc(sizeof(struct loader_impl_wasm_type));

	if (wasm_impl == NULL)
	{
		log_write("metacall", LOG_LEVEL_ERROR, "WebAssembly loader: Failed to allocate memory for loader implementation");
		goto error_impl_alloc;
	}

	if (wasm_loader_impl_initialize_types(impl) != 0)
	{
		log_write("metacall", LOG_LEVEL_ERROR, "WebAssembly loader: Failed to initialize types");
		goto error_types_init;
	}

	// TODO: Use wasm_engine_new_with_config function for passing in config?
	wasm_impl->engine = wasm_engine_new();

	if (wasm_impl->engine == NULL)
	{
		log_write("metacall", LOG_LEVEL_ERROR, "WebAssembly loader: Failed to create engine");
		goto error_engine_creation;
	}

	wasm_impl->store = wasm_store_new(wasm_impl->engine);

	if (wasm_impl->store == NULL)
	{
		log_write("metacall", LOG_LEVEL_ERROR, "WebAssembly loader: Failed to create store");
		goto error_store_creation;
	}

	wasm_impl->paths = vector_create_type(loader_naming_path);

	if (wasm_impl->paths == NULL)
	{
		log_write("metacall", LOG_LEVEL_ERROR, "WebAssembly loader: Failed to create paths vector");
		goto error_paths_creation;
	}

	loader_initialization_register(impl);
	log_write("metacall", LOG_LEVEL_DEBUG, "WebAssembly loader initialized correctly");

	return wasm_impl;

error_paths_creation:
	wasm_store_delete(wasm_impl->store);
error_store_creation:
	wasm_engine_delete(wasm_impl->engine);
error_engine_creation:
error_types_init:
	free(wasm_impl);
error_impl_alloc:
	return NULL;
}

int wasm_loader_impl_execution_path(loader_impl impl, const loader_naming_path path)
{
	loader_impl_wasm wasm_impl = loader_impl_get(impl);
	vector_push_back_empty(wasm_impl->paths);
	loader_naming_path *back_ptr = vector_back(wasm_impl->paths);
	strncpy(*back_ptr, path, sizeof(loader_naming_path));

	return 0;
}

FILE *wasm_loader_impl_open_file_absolute(const char *path, size_t *file_size)
{
	FILE *file = fopen(path, "rb");
	if (file == NULL)
	{
		goto error_open_file;
	}

	if (fseek(file, 0, SEEK_END) != 0)
	{
		log_write("metacall", LOG_LEVEL_ERROR, "WebAssembly loader: Failed to seek to end of file");
		goto error_seek_file;
	}

	*file_size = ftell(file);

	if (fseek(file, 0, SEEK_SET) != 0)
	{
		log_write("metacall", LOG_LEVEL_ERROR, "WebAssembly loader: Failed to seek to start of file");
		goto error_seek_file;
	}

	return file;

error_seek_file:
	fclose(file);
error_open_file:
	return NULL;
}

FILE *wasm_loader_impl_open_file_relative(loader_impl_wasm impl, const char *path, size_t *file_size)
{
	for (size_t i = 0; i < vector_size(impl->paths); i++)
	{
		loader_naming_path *exec_path = vector_at(impl->paths, i);

		// FIXME: This could fail if the resulting path is longer than sizeof(loader_naming_path)
		loader_naming_path abs_path;
		(void)loader_path_join(*exec_path, strlen(*exec_path) + 1, path, strlen(path) + 1, abs_path);

		FILE *file = wasm_loader_impl_open_file_absolute(abs_path, file_size);

		if (file == NULL)
		{
			log_write("metacall", LOG_LEVEL_DEBUG, "WebAssembly loader: Could not open file %s", abs_path);
			continue;
		}

		log_write("metacall", LOG_LEVEL_DEBUG, "WebAssembly loader: Opened file %s", abs_path);
		return file;
	}

	return NULL;
}

loader_impl_wasm_handle wasm_loader_impl_create_handle(size_t num_modules)
{
	loader_impl_wasm_handle handle = malloc(sizeof(struct loader_impl_wasm_handle_type));

	if (handle == NULL)
	{
		log_write("metacall", LOG_LEVEL_ERROR, "WebAssembly loader: Failed to allocate memory for handle");
		goto error_handle_alloc;
	}

	handle->modules = vector_create_reserve_type(loader_impl_wasm_module, num_modules);

	if (handle->modules == NULL)
	{
		log_write("metacall", LOG_LEVEL_ERROR, "WebAssembly loader: Failed to allocate memory for modules");
		goto error_modules_alloc;
	}

	return handle;

error_modules_alloc:
	free(handle);
error_handle_alloc:
	return NULL;
}

int wasm_loader_impl_handle_add_module(loader_impl impl, loader_impl_wasm_handle handle, const char *buffer, size_t size)
{
	loader_impl_wasm wasm_impl = loader_impl_get(impl);
	// There is sadly no way to check whether `wasm_byte_vec_new`
	// fails, so we just have to hope for the best here.
	wasm_byte_vec_t binary;
	wasm_byte_vec_new(&binary, size, buffer);

	// TODO: Maybe create module in-place in vector instead
	loader_impl_wasm_module module;
	module.module = wasm_module_new(wasm_impl->store, &binary);

	wasm_byte_vec_delete(&binary);

	// TODO: `wasm_module_new` can fail for a multitude of reasons.
	//       Consider using the Wasmtime-specific `wasmtime_module_new`,
	//       which provides richer error messages. This could be done
	//       conditionally using the preprocessor to maintain compatibility
	//       with other runtimes.
	if (module.module == NULL)
	{
		log_write("metacall", LOG_LEVEL_ERROR, "WebAssembly loader: Failed to create module");
		wasm_loader_impl_clear(impl, handle);
		return 1;
	}

	// TODO: Add support for imports. This currently results in undefined
	//       behavior if the module expects imports.

	// No way to check whether `wasm_instance_new` fails, so hope for the best
	// here too.
	wasm_extern_vec_t imports = WASM_EMPTY_VEC;
	module.instance = wasm_instance_new(wasm_impl->store, module.module, &imports, NULL);
	wasm_instance_exports(module.instance, &module.exports);

	vector_push_back_var(handle->modules, module);

	return 0;
}

int wasm_loader_impl_load_module_from_file(loader_impl impl, loader_impl_wasm_handle handle, const char *path)
{
	int ret = 1;

	loader_impl_wasm wasm_impl = loader_impl_get(impl);

	size_t file_size;
	FILE *file = wasm_loader_impl_open_file_relative(wasm_impl, path, &file_size);

	if (file == NULL)
	{
		log_write("metacall", LOG_LEVEL_ERROR, "WebAssembly loader: Failed to open file %s", path);
		goto error_open_file;
	}

	char *buffer = malloc(file_size);
	if (buffer == NULL)
	{
		log_write("metacall", LOG_LEVEL_ERROR, "WebAssembly loader: Failed to allocate file buffer");
		goto error_buffer_alloc;
	}

	size_t bytes_read = fread(buffer, 1, file_size, file);
	if (bytes_read != file_size)
	{
		log_write("metacall", LOG_LEVEL_ERROR, "WebAssembly loader: Failed to read file (read %zu of %zu bytes)", bytes_read, file_size);
		goto error_read_file;
	}

	if (wasm_loader_impl_handle_add_module(impl, handle, buffer, file_size) != 0)
	{
		goto error_add_module;
	}

	ret = 0;

error_add_module:
error_read_file:
	free(buffer);
error_buffer_alloc:
	fclose(file);
error_open_file:
	return ret;
}

loader_handle wasm_loader_impl_load_from_file(loader_impl impl, const loader_naming_path paths[], size_t size)
{
	loader_impl_wasm_handle handle = wasm_loader_impl_create_handle(size);

	if (handle == NULL)
	{
		return NULL;
	}

	for (size_t idx = 0; idx < size; idx++)
	{
		if (wasm_loader_impl_load_module_from_file(impl, handle, paths[idx]) != 0)
		{
			return NULL;
		}
	}

	return handle;
}

loader_handle wasm_loader_impl_load_from_memory(loader_impl impl, const loader_naming_name name, const char *buffer, size_t size)
{
	(void)name;

	loader_impl_wasm_handle handle = wasm_loader_impl_create_handle(1);

	if (handle == NULL || wasm_loader_impl_handle_add_module(impl, handle, buffer, size) != 0)
	{
		return NULL;
	}

	return handle;
}

loader_handle wasm_loader_impl_load_from_package(loader_impl impl, const loader_naming_path path)
{
	/* TODO */

	(void)impl;
	(void)path;

	return NULL;
}

int wasm_loader_impl_clear(loader_impl impl, loader_handle handle)
{
	(void)impl;

	loader_impl_wasm_handle wasm_handle = (loader_impl_wasm_handle)handle;

	// TODO: Refactor into separate function
	for (size_t idx = 0; idx < vector_size(wasm_handle->modules); idx++)
	{
		loader_impl_wasm_module *module = vector_at(wasm_handle->modules, idx);
		wasm_extern_vec_delete(&module->exports);
		wasm_instance_delete(module->instance);
		wasm_module_delete(module->module);
	}

	vector_destroy(wasm_handle->modules);
	free(wasm_handle);

	return 0;
}

type wasm_loader_impl_val_kind_to_type(loader_impl impl, wasm_valkind_t kind)
{
	switch (kind)
	{
		case WASM_I32:
			return loader_impl_type(impl, "i32");
		case WASM_I64:
			return loader_impl_type(impl, "i64");
		case WASM_F32:
			return loader_impl_type(impl, "f32");
		case WASM_F64:
			return loader_impl_type(impl, "f64");
		default:
			return NULL;
	}
}

char *wasm_loader_impl_get_export_type_name(const wasm_exporttype_t *export_type)
{
	const wasm_name_t *name = wasm_exporttype_name(export_type);
	char *null_terminated_name = malloc(name->size + 1);

	if (null_terminated_name == NULL)
	{
		log_write("metacall", LOG_LEVEL_ERROR, "WebAssembly loader: Failed to allocate memory for name of export type");
		return NULL;
	}

	strncpy(null_terminated_name, name->data, name->size);
	null_terminated_name[name->size] = '\0';

	return null_terminated_name;
}

int wasm_loader_impl_discover_function(loader_impl impl, scope scp, const wasm_externtype_t *extern_type, const char *name, const wasm_extern_t *extern_val)
{
	const wasm_functype_t *func_type =
		wasm_externtype_as_functype_const(extern_type);
	const wasm_valtype_vec_t *params = wasm_functype_params(func_type);
	const wasm_valtype_vec_t *results = wasm_functype_results(func_type);

	loader_impl_wasm_function func_impl = malloc(sizeof(struct loader_impl_wasm_function_type));
	if (func_impl == NULL)
	{
		log_write("metacall", LOG_LEVEL_ERROR, "WebAssembly loader: Failed to allocate memory for function handle");
		return 1;
	}

	func_impl->func = wasm_extern_as_func_const(extern_val);

	function func = function_create(name, params->size, func_impl, &function_wasm_singleton);
	signature sig = function_signature(func);

	if (results->size > 0)
	{
		// TODO: Add support for multiple return values
		if (results->size > 1)
		{
			log_write("metacall", LOG_LEVEL_WARNING, "WebAssembly loader does not support multiple return values yet, using first return value");
		}

		signature_set_return(sig,
			wasm_loader_impl_val_kind_to_type(impl, wasm_valtype_kind(results->data[0])));
	}

	for (size_t param_idx = 0; param_idx < params->size; param_idx++)
	{
		// TODO: We supply an empty string as the parameter name, since
		//       `signature_set' doesn't allow `NULL'. Consider changing
		//       the implementation of `signature_set` to allow this.
		signature_set(sig, param_idx, "",
			wasm_loader_impl_val_kind_to_type(impl, wasm_valtype_kind(params->data[param_idx])));
	}

	scope_define(scp, function_name(func), value_create_function(func));

	return 0;
}

//int wasm_loader_impl_discover_module(loader_impl impl, scope scp, const wasm_module_t *module, const wasm_instance_t *instance)
int wasm_loader_impl_discover_module(loader_impl impl, scope scp, const loader_impl_wasm_module *module)
{
	int ret = 0;

	// There is no way to check whether `wasm_module_exports` fails, so just
	// hope for the best.
	wasm_exporttype_vec_t export_types;
	wasm_module_exports(module->module, &export_types);

	for (size_t export_idx = 0; export_idx < export_types.size; export_idx++)
	{
		// All of the `wasm_*` calls in this loop return pointers to memory
		// owned by `export_types`, so no need to do any error checking or
		// cleanup.
		const wasm_externtype_t *extern_type = wasm_exporttype_type(export_types.data[export_idx]);
		const wasm_externkind_t kind = wasm_externtype_kind(extern_type);
		char *export_name = wasm_loader_impl_get_export_type_name(export_types.data[export_idx]);

		if (export_name == NULL)
		{
			ret = 1;
			goto cleanup;
		}

		// TODO: Do we need to implement the other types as well?
		if (kind == WASM_EXTERN_FUNC)
		{
			// There is a 1-to-1 correspondence between between the instance
			// exports and the module exports, so we can reuse the index.
			wasm_loader_impl_discover_function(impl, scp, extern_type, export_name, module->exports.data[export_idx]);
		}

		free(export_name);
	}

cleanup:
	wasm_exporttype_vec_delete(&export_types);
	return ret;
}

int wasm_loader_impl_discover(loader_impl impl, loader_handle handle, context ctx)
{
	loader_impl_wasm_handle wasm_handle = (loader_impl_wasm_handle)handle;
	scope scp = context_scope(ctx);

	for (size_t idx = 0; idx < vector_size(wasm_handle->modules); idx++)
	{
		wasm_loader_impl_discover_module(impl, scp, vector_at(wasm_handle->modules, idx));
	}

	return 0;
}

int wasm_loader_impl_destroy(loader_impl impl)
{
	loader_impl_wasm wasm_impl = loader_impl_get(impl);
	loader_unload_children(impl);
	vector_destroy(wasm_impl->paths);
	wasm_store_delete(wasm_impl->store);
	wasm_engine_delete(wasm_impl->engine);
	free(wasm_impl);

	log_write("metacall", LOG_LEVEL_DEBUG, "WebAssembly loader destroyed");

	return 0;
}
