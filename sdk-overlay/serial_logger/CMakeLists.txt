# Copyright (C) 2020 QUECTEL Technologies Limited and/or its affiliates("QUECTEL").
# All rights reserved.

set(target ql_app_serial_logger)

add_library(${target} STATIC)
set_target_properties(${target} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${out_app_lib_dir})
target_compile_definitions(${target} PRIVATE OSI_LOG_TAG=LOG_TAG_QUEC)
target_include_directories(${target} PUBLIC
	dani_log
	setting
	dani_sys
	ring_buffer
	pkt_queue
	command
	main
	sys_proc
	input_log_proc
	mqtt_proc
	sd_proc
	sms_proc
	io_proc

	${SOURCE_TOP_DIR}/components/calclib/include
	${SOURCE_TOP_DIR}/components/ql-application/peripheral/inc)

target_link_libraries(${target} PUBLIC calclib_crc)

target_sources(${target} PRIVATE
	dani_log/dani_log.c
	setting/setting.c
	dani_sys/dani_sys.c
	ring_buffer/ring_buffer.c
	pkt_queue/pkt_queue.c
	command/command.c
	main/main.c
	sys_proc/sys_proc.c
	input_log_proc/input_log_proc.c
	mqtt_proc/mqtt_proc.c
	sms_proc/sms_proc.c
	sd_proc/sd_proc.c
	io_proc/io_proc.c

)

relative_glob(srcs */*.h */*.c)
beautify_c_code(${target} ${srcs})
