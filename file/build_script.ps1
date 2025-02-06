param ($buildtype)

$test_set = $PWD -replace '\\', '/'    
$test_set = $test_set + '/app/mcuboot.conf'

if($buildtype -eq "clean")
{
    write-host "Building clean"
    west build --pristine=always -b anne_aria_rev_c_cpuapp app/ -- -Dmcuboot_CONF_FILE="$test_set" 
}
else 
{
    write-host "Do a quick build"
    west build -b anne_aria_rev_c_cpuapp app/ -- -Dmcuboot_CONF_FILE="$test_set"
}
