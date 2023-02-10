local input, output = ...

local stats = 0
local result = {}
for line in io.lines(input) do
    if stats == 0 then
        if line == 'Contents of section __TEXT,__text:' then
            stats = 1
        end
    else
        if line:sub(1, 6):match "^ [0-9a-f][0-9a-f][0-9a-f][0-9a-f] $" then
            local hexstring = line:sub(7, 7 + 9 * 4)
            for hex in hexstring:gmatch "[0-9a-f][0-9a-f]" do
                result[#result + 1] = "\\x" .. hex
            end
        else
            if line ~= 'Contents of section __TEXT,__cstring:' then
                stats = 0
            end
        end
    end
    if line == 'Contents of section __TEXT,__stubs:' then
        io.stderr:write("please chek shellcode find the dependency on external functions\n")
        os.exit(1)
    end
end

local f <close> = assert(output and io.open(output, "w") or io.stdout)
f:write("const char inject_code[] = {\n")
f:write('"')
f:write(table.concat(result))
f:write('"\n')
f:write("};\n")
