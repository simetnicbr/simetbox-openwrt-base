require("uci")

local server_host = 'https://as-pool.simet.nic.br'

--testado
function is_simet_box()
    return file_exists('/sbin/sysupgrade')
end

--testado
function read_file_content(path)
    local f = io.open(path, "r")
    local content = f:read("*all")
    f:close()
    return content
end

--testado
function write_temp_file(prefix, content)
    local filename = read_from_bash('mktemp -t '..prefix..'.XXXXXXX')
    os.execute('chmod 644 '..filename)
    local file = io.open(filename, "w")
    file:write(content)
    file:flush()
    return file, filename
end

--testado
function remove_file(filename, file)
    if file then
        file:close()
    end
    read_from_bash('rm '..filename)
end

--testado
function json_encode(content)
    
    local json_message = ''
    if is_simet_box() then 
        json = require('luci.json')
        json_message = json.encode(content)
    else
        JSON = require('simet.JSON')
        json_message = JSON:encode(content)
    end

    return json_message

end

--testado
function json_decode(json_string)

    local obj = {}
    if is_simet_box() then 
        local json = require('luci.json')
        obj = json.decode(json_string)
    else
        local JSON = require('simet.JSON')
        obj = JSON:decode(json_string)
    end

    return obj
end

function current_config_cursor()
    return uci.cursor()
end

function factory_config_cursor()
    local factory_cursor = uci.cursor()
    factory_cursor:set_confdir('/etc/factory_config')
    return factory_cursor
end


--testado
function get_valid_cursor(cursor)
    if not cursor then
        cursor = uci.cursor()
    end
    return cursor
end

function add_uci_section_(config,section_type,cursor)
    cursor = get_valid_cursor(cursor)
    return cursor:add(config, section_type)
end

function add_uci_section(config,section_type,cursor)
    cursor = get_valid_cursor(cursor)
    local name =  add_uci_section_(config,section_type,cursor)
    commit_uci_config(config,cursor)
    return name
end

function add_uci_named_section_(config,section_name,section_type,cursor)
    cursor = get_valid_cursor(cursor)
    cursor:set(config, section_name, section_type)
end

function add_uci_named_section(config,section_name,section_type,cursor)
    cursor = get_valid_cursor(cursor)
    add_uci_named_section_(config,section_name,section_type,cursor)
    commit_uci_config(config,cursor)
end

function write_uci_option_(config,section,option,value,cursor)
    cursor = get_valid_cursor(cursor)
    
    if not read_uci_section(config,section,cursor) then
        cursor:set(config,section,section) 
    end
    cursor:set(config,section,option,value) 
end

function write_uci_option(config,section,option,value,cursor)
    cursor = get_valid_cursor(cursor)
    write_uci_option_(config,section,option,value,cursor)
    commit_uci_config(config,cursor)
end

function delete_uci_section_(config,section,cursor)
    cursor = get_valid_cursor(cursor)
    cursor:delete(config,section)
end

function delete_uci_section(config,section,cursor)
    cursor = get_valid_cursor(cursor)
    delete_uci_section_(config,section,cursor)
    commit_uci_config(config,cursor)
end

function delete_uci_option_(config,section,option,cursor)
    cursor = get_valid_cursor(cursor)
    cursor:delete(config,section,option)
end

function delete_uci_option(config,section,option,cursor)
    cursor = get_valid_cursor(cursor)
    delete_uci_option_(config,section,option,cursor)
    commit_uci_config(config,cursor)
end

function commit_uci_config(config,cursor)
    cursor = get_valid_cursor(cursor)
    cursor:commit(config)
end


function read_uci_option(config,section,option,cursor)
    cursor = get_valid_cursor(cursor)
    return cursor:get(config,section,option) 
end

function read_uci_section(config,section,cursor)
    cursor = get_valid_cursor(cursor)
    return cursor:get(config,section) 
end

--testado
function create_config_file(name)
    os.execute('touch /etc/config/'..name..' ; touch /etc/factory_config/'..name)
end

--testado
function read_from_bash(cmd)    
    local content = ''                                                                                                                          
    local util = io.popen(cmd)                                                                                                           
    if util then                                                                                                                                          
        local ln = util:read("*l")                                                                                                        
        content = ln                                                                                                              
        util:close()                                                                                                                                  
    end                                                                                                                                                   
    return  content                                                                                                                                  
end

--testado
function file_exists(name)
   local f=io.open(name,"r")
   if f~=nil then io.close(f) return true else return false end
end

--testado
function is_valid(...)
    local valid = true
    local value
    for n=1,select('#',...) do
        value = select(n,...)
        valid = valid and value and #trim(value) > 0 and value ~= '(null)' and value ~= 'false' and value ~= 404 and value ~= '404' and value ~= 0 and value ~= '0'
    end
    return valid
end

--testado
function trim(s)
    if s then 
        s = tostring(s)
        return s:gsub("^%s*(.-)%s*$","%1")
    else 
        return '' 
    end
end

--testado
function is_user_router()
    local owner = get_owner(get_mac_address()) 
    if is_valid(owner) then 
        owner = owner:gsub("^%s*(.-)%s*$","%1")
    else 
        owner = ''
    end

    return #owner > 0 and owner ~= 'nic' and owner ~= 'isp'
end

--testado
function is_nic_router()
    local owner = get_owner(get_mac_address()) 
    if is_valid(owner) then 
        owner = owner:gsub("^%s*(.-)%s*$","%1")
    else 
        owner = ''
    end

    return #owner == 0 or owner == 'nic'
end

--testado
function fetch_owner(mac_address)
    if not mac_address then
        return false
    end
    return read_from_bash("simet_ws "..server_host.."/simet-box-history/GetOwner?hashDevice="..mac_address)
end

--testado
function get_owner(mac_address)
    return read_uci_option('system','ownership','owner')
end

--testado
function get_device_version()
    return read_from_bash("get_simet_box_version.sh")                                                                                                                                                                                                                                  
end

--testado
function get_device_model()
    return read_from_bash("get_model.sh")
end 

--testado
function get_device_hash()
    return read_from_bash("cat /etc/config/hash")
end

--testado
function get_mac_address()
    return read_from_bash("get_mac_address.sh")                                                                                                                                                                                                                                                
end

--testado
function is_online()                       
    local result = read_from_bash("ping -c 1 as-pool.simet.nic.br | grep \"64 bytes\" | awk '{print $1}'")
    if result == '64' then
        return true    
    else
        return false                   
    end
end   

--testado 
function is_integer(n)
    return n and tonumber(n) and tonumber(n) == math.floor(tonumber(n))
end

--testado
function sleep(n)
  if is_integer(n) and tonumber(n) >= 0  then
    os.execute("sleep " .. tonumber(n))
    return true
  end
  return false
end

function reboot()
  os.execute("reboot")
end

function get_pid(process)
    local pid
    handle = io.popen("ps -w | grep " .. process .. " | awk '{ print $1  }' ")
    pid = handle:read()
    handle:close()
    return pid
end

--testado
function is_empty_value(value)
    return (value == nil or string.len(trim(value)) == 0)
end

function reload_services(services)
    os.execute("/sbin/luci-reload "..table.concat(services, " ").." > /dev/null 2>&1")
end

function reload_service(service)
    os.execute("/sbin/luci-reload "..service.." > /dev/null 2>&1")
end

function is_valid_ip_address(ip)
    local valid = false

    local a,b,c,d=ip:match("^(%d%d?%d?)%.(%d%d?%d?)%.(%d%d?%d?)%.(%d%d?%d?)$")
    a=tonumber(a)
    b=tonumber(b)
    c=tonumber(c)
    d=tonumber(d)

    if (a ~= nil and b ~= nil and c ~= nil and d ~= nil and a >= 0 and a <= 255 and b >= 0 and b <= 255 and c >= 0 and c <= 255 and d >= 0 and d <= 255) then
        valid = true
    end

    return valid
end


function is_valid_port(port)
    local valid = false

    a=tonumber(port)

    if (a ~= nil and a >= 1 and a <= 64*1024) then
        valid = true
    end

    return valid
end
