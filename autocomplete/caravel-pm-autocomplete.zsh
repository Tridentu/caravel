 
function caravel_autocomp(){
    comadd create-package find-packages install-packages uninstall-package list-installed-packages
    _describe 'command' "('create-package:Creates a Caravel package.' 'find-packages:Retrieves packages from the CMR (Caravel Main Repository) or any other repository recognized by the OS.' 'uninstall-package:Uninstalls the given package on your system' 'list-installed-packages:Lists Caravel packages that have uninstallers (lua scripts for the job) alreay registered.' 'add-repo' 'adds a repository to the system')"
}

compdef caravel_autocomp caravel-pm
