key = {
    [1] = {1, 2, 3},
    ['size'] = {a=1,b=2},
}
print(_VERSION)

function test(...)
    local n = ...
    print(_VERSION)
    key = 2
end
test(1, "2s")
key = 4
