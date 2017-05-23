require('simet.simet_utils')

local simetbox = is_simet_box()

local args = {...} 

--[[

local staging_dir = '/tmp/staging_config/'
local staging_info_path = staging_dir..'staging_info'
local separator_symbol = '->'

os.execute( "mkdir -p "..staging_dir )


function free_overlay_space()

	local handle = io.popen('df')

	local space = 0

	for line in handle:lines() do
		for val1,val2 in string.gmatch(line, ".*%s+([%w%p]+)%s.*%s(/)$" ) do
			space = val1
		end
	end

	handle:close()

	return tonumber(space)

end

function get_file_size(file_path)

	if not file_exists(file_path) then
		return 0
	end

	local handle = io.popen("wc -c "..file_path)
	
	wc_string = handle:read()
	size = 0
	for v in string.gmatch(wc_string, "(%d+).*" ) do
		size = v
	end

	handle:close()

	return size

end

function check_available_space(table)
	local staging_files_size = 0
	local dst_files_size = 0
	local free_space = free_overlay_space()
	
	for staging_file, dst_file in pairs(table) do
	    print(staging_dir..staging_file)
	    staging_files_size = staging_files_size + get_file_size(staging_dir..staging_file)
	    dst_files_size = dst_files_size + get_file_size(dst_file)
    end

    print(staging_files_size)
    print(dst_files_size)
   	print(free_space)

   	return (staging_files_size < free_space)
	  
end

function copy_files(table)

	for k, v in pairs(table) do
        os.execute( "cp "..staging_dir..k.." "..v )
    end

end

function check_files_in_staging_area(table)
	local valid = true
	for k, v in pairs(table) do
		if not file_exists(staging_dir..k) then
			valid = false
		end
	end
	return valid
end

function read_staging_info()

	local filename_pattern = "[%p,-,_,%a,%d,%w]+"	
	local file = io.open(staging_info_path, "a+")

	local count = 0
	local lines = {}
	local table = {}

	for line in file:lines() do 
		count = count + 1
		lines[count] = line
		for k,v in string.gmatch(line, "("..filename_pattern..")%s*"..separator_symbol.."%s*("..filename_pattern..")" ) do
			table[k] = v
		end
	end

	io.close(file)


	return table
end

function commit()

	local table = read_staging_info()

	
	if check_files_in_staging_area(table) and check_available_space(table) then
	   	copy_files(table)
	else
		print('Erro ao commitar arquivos')
	end
	
	--clear()
	print('commit')
end

function add_file(staging_filename, file_path)
	
	if not (staging_filename and file_path) then
		print("Invalid parameters")
		return
	end

	local file = io.open (staging_info_path, "a+")
    io.output(file)
    io.write(staging_filename.." "..separator_symbol.." "..file_path.."\n")
    io.close(file)
	
	print(staging_dir..staging_filename)
	file = io.open(staging_dir..staging_filename,"w+")
	io.close(file)	

    print("add_file: "..staging_filename.." to "..file_path)
end

function clear() 
    os.execute("rm -f "..staging_dir.." *")
end

]]

function add_section(cursor, config, section)

	local section_name = section['section_name']
	local section_type = section['section_type']

	if section_name then
		add_uci_named_section_(config, section_name, section_type, cursor)
	else
		section_name = add_uci_section_(config, section_type, cursor)
	end

	return add_section
end

function is_index_valid(index)
	return index >= 1
end

function get_section_name(cursor, config, section, action)

	local count
	local section_type = section['section_type']
	local section_index = section['section_index']
	local use_last = false
	local section_name

	if section_index and section_index <= 0 then
		use_last = true
	end

	if section['section_name'] then
		section_name = section['section_name']
		if action == 'update' then
			add_uci_named_section_(config, section_name, section_type, cursor)
		end
	elseif section_index then
		count = 1
		cursor:foreach(config, section_type, function(s)  
			if count == section_index or use_last then
				section_name = s['.name']
			end
			count = count + 1
		end)
	end	

	if not section_name and use_last and section_type then
		section_name = add_uci_section_(config, section_type, cursor)
	end

	return section_name

end


function keep_last(cursor,config,section)
	
	local section_names = {}
	count = 0
	cursor:foreach(config, section['section_type'], function(s)  
		count = count + 1
		section_names[count] = s['.name']
	end)

	for i=1,count-1 do 
		delete_uci_section_(config, section_names[i], cursor)
	end

end

function remove_all(cursor,config,section)
	
	local section_names = {}
	count = 0
	cursor:foreach(config, section['section_type'], function(s)  
		count = count + 1
		section_names[count] = s['.name']
	end)

	for i=1,count do 
		delete_uci_section_(config, section_names[i], cursor)
	end

end

function apply_section_changes(cursor, config, section, action, section_name)

	local section_name = get_section_name(cursor, config, section, action)


	if not section_name then
		print('Section name needed')
		return
	end


	local values = section['values']

	if values then
		for key, value in pairs(values) do
			if action == 'update' then
				write_uci_option_(config, section_name, key, value, cursor)
			elseif action == 'remove' then
				delete_uci_option_(config, section_name, key, cursor)
			end
		end
	else
		if action == 'remove' then
			delete_uci_section_(config, section_name, cursor)
		end
	end
end


function alter_section(cursor, config, section, action)


	if action == 'keep_last' then
		keep_last(cursor,config,section)
	elseif action == 'remove_all' then
		remove_all(cursor,config,section)
	elseif action == 'add' then
		add_section(cursor, config, section)
	else
		apply_section_changes(cursor, config, section, action)
	end

end

function alter_config(cursor,config,sections,action)

	create_config_file(config)

	for index,section in pairs(sections) do
		alter_section(cursor,config,section,action)
	end


end

function parse_config(config_obj)

	local cursors = {}
	cursors['current'] = current_config_cursor()
	cursors['factory'] = factory_config_cursor()

	local config = config_obj['config']
	local action = config_obj['action']
	local sections = config_obj['sections']
	local locations = config_obj['location']

	if action == 'reset' then
		return
	end

	if not locations or #locations == 0 then
		locations = {}
		locations[0] = 'current'
	end

	for index,location in pairs(locations) do
		local cursor = cursors[location]
		alter_config(cursor, config, sections, action)
		commit_uci_config(config,cursor)
	end
	
end


function json_reload_services(json_string)

	if (json_string == nil) then
		print('Invalid JSON object')
		return
	end

	local reset = false

	local configs = json_decode(json_string)

	local config_names = {}

	for index,config in pairs(configs) do

		if config['action'] == 'reset' then
			reset = true
		end

		local name = config['config']
		if name == 'wireless' then
			name = 'network'
		end

		config_names[index] = name
	end
		

	if reset then 
		reboot()
	else
		if #config_names > 0 then
			reload_services(config_names)
		end
	end

	return 1

end

function json_apply_configs(json_string)


	if (json_string == nil) then
		print('Invalid JSON object')
		return
	end

	local configs = json_decode(json_string)

	for index,config in pairs(configs) do
		parse_config(config)
	end
	
	return 1

end

function parse_command(args)

	local command = args[1]

	local status = 0

	--[[if command == 'commit' then
		status = commit()
	elseif (command == 'add_file') then
		local staging_file_name = args[2]
	    local file_path = args[3]
		status = add_file(staging_file_name, file_path)
	elseif (command == 'clear') then
		status = clear()
		]]
	if (command == 'json_apply_configs') then
		local json_string = read_file_content(args[2])
		status = json_apply_configs(json_string)
	elseif (command == 'json_reload_services') then
		local json_string = read_file_content(args[2])
		status = json_reload_services(json_string)
	else
		print('Invalid Command')
		status = -1
	end

	return status

end

print(parse_command(args))	
