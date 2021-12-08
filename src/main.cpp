
#include <caravel/CaravelWriter.h>
#include <caravel/CaravelUtils.h>
#include <caravel/CaravelReader.h>
#include <caravel/CaravelAuthor.h>
#include <caravel/CaravelContext.h>
#include <caravel/CaravelDownloader.h>
#include <caravel/CaravelDBContext.h>
#include <string>
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <CLI11.hpp>
#include <vector>
#include <mast_tk/core/LineUtils.h>

int main(int argc, char** argv){

  CLI::App caravelApp;

  caravelApp.require_subcommand(1);

  {
    auto createPackage = caravelApp.add_subcommand("create-package","Creates a .caravel package for distribution using the given folder.");

    std::string lua_script_install, lua_script_uninstall;
    std::string packageDir;
    createPackage->add_option("packagedir",packageDir,"The package directory (relative to the current working directory)")->required();

    std::string packageType;
    createPackage->add_option("type",packageType,"The type of Caravel package to create (binaries, source, config or assets).")->required()->check(CaravelPM::CaravelPkgClass);

    createPackage->callback([&](){
      std::filesystem::path path_pkg = ((std::filesystem::current_path() +=  "/") += packageDir);
      if (!std::filesystem::exists(path_pkg)){
	std::cout << "Package Directory does not exist." << std::endl;
	return 0;
      }
      std::cout << "Creating package..." << std::endl;
      if(packageType == "binaries"){
	std::cout << "Binary format detected." << std::endl;
	CaravelPM::CaravelAuthor::CreatePackage(packageDir,CaravelPM::CaravelPkgType::Binaries);
	std::cout << "Package " << packageDir << " Created (" << packageDir << ".caravel" << ")" << std::endl;
      } else if (packageType == "config") {
	std::cout << "Dotfiles detected." << std::endl;
	CaravelPM::CaravelAuthor::CreatePackage(packageDir,CaravelPM::CaravelPkgType::DotFiles);
	std::cout << "Package " << packageDir << " Created (" << packageDir << ".caravel" << ")" << std::endl;
      }
    
    });

  }

  {
    auto findPackages = caravelApp.add_subcommand("find-packages", "Finds packages from a given keyword or query.");
    std::string query;
    findPackages->add_option("packagequery", query, "The keyword or or query to find packages with.")->required();
    findPackages->callback([&](){
      CaravelPM::CaravelDBContext::InitDB("https://tridentu.github.io/acquirium/pman.caraveldb");
      std::vector<CaravelPM::CaravelPackageInfo> infos = CaravelPM::CaravelDBContext::GetDB()->FindPackagesFromNameQuery(query);
      if(infos.empty())
        std::cout << "No packages found.";
      else  {
        std::cout << "---------- " << infos.size() <<  " PACKAGES FOUND ----------" << std::endl;
        for (int i = 0; i < infos.size(); i++){
          std::cout << "---------- " << infos.at(i).PackageName << "  ----------" << std::endl;
          std::cout << "Package Name: " << infos.at(i).PackageName << std::endl;
          std::cout << "Package Type: " << infos.at(i).PackageType << std::endl;
          std::cout << "Package Namespace: " << infos.at(i).Namespace << std::endl;
          std::cout << "Package Category: " << infos.at(i).PackageCategory << std::endl;
          std::cout << "Package Description: " << infos.at(i).Description << std::endl;
          std::cout << "---------- end ----------" << std::endl;

        }
        std::cout << "-------------------------------" << std::endl;
      }
      return 0;
    });
  }

  {
    auto installPackage = caravelApp.add_subcommand("install-package","Installs a caravel package.");
    bool local_package;
    installPackage->add_flag("--local",local_package,"Uses a local file instead of a package in a repository.");

    std::string packageName;
    installPackage->add_option("packagename", packageName, "The name of the package (or archive) to install.")->required();
  
    installPackage->callback([&](){
      CaravelPM::CaravelDBContext::InitDB("https://tridentu.github.io/acquirium/pman.caraveldb");
      if(local_package){
	std::filesystem::path path_pkg = std::filesystem::current_path() / std::string(packageName + ".caravel");
	CaravelPM::CaravelReader* reader = new CaravelPM::CaravelReader(path_pkg.string(),std::string(packageName + ".caravel"));
      
	if(!reader->Extract()){
	  std::cerr << "Can't extract package." << std::endl;
	  return 0;
	}
	std::cout << "Installing " << reader->GetMetadata("name") << std::endl;
	if(!reader->Install()){
	  std::cerr << "Can't install package " << reader->GetMetadata("name") << std::endl;
	  return 0;
	}
	delete reader;
	return 0;
      } else {
	// Check if the package exists.
	std::string packageNamespace = CaravelPM::CaravelDBContext::GetDB()->FindNamespace(packageName);
	if(packageNamespace.empty()){
	  std::cerr << "Can't download package; " << packageName << " doesn't exist." << std::endl;
	  return 0;
	}
	std::cout << "Downloading Caravel Package " << packageName << "..." << std::endl;
	std::string url = CaravelPM::CaravelDBContext::GetDB()->GetPackageLink(packageName); 
	CaravelPM::CaravelDownloader* downloader = new CaravelPM::CaravelDownloader(packageName, url);
		
	downloader->Run();
	std::cout << "Installing Caravel Package " << packageName << "..." << std::endl;
	std::filesystem::path path_pkg = std::filesystem::path(std::string(getenv("HOME")) + std::string("/" + packageName + ".caravel"));
	CaravelPM::CaravelReader* reader = new CaravelPM::CaravelReader(path_pkg.string(),std::string(packageName + ".caravel"));
	if(!reader->Extract()){
	  std::cerr << "Can't extract package." << std::endl;
	  return 0;
	}
	if(!reader->Install()){
	  std::cerr << "Can't install package " << reader->GetMetadata("name") << std::endl;
	  return 0;
	}
	std::cout << "Done." << std::endl;
	delete reader;
	return 0;
      }
    });
  }

  {
  auto uninstallPackage = caravelApp.add_subcommand("uninstall-package","Uninstalls the given caravel packages");

  std::string packageName;
  uninstallPackage->add_option("package",packageName,"The package to install.")->required();

  uninstallPackage->callback([&](){
    std::string uninstallScript("/usr/share/caravel-uninstall/" + packageName + ".lua");
    std::filesystem::path ulPath(uninstallScript);
   
    CaravelPM::CaravelContext* context = new CaravelPM::CaravelContext(ulPath.string());
    context->Run();
    delete context;

    std::cout << "Either " << packageName << " was uninstalled or failed to uninstall.";
    return 0;
  });

  caravelApp.footer("Caravel v0.2.0");

  CLI11_PARSE(caravelApp, argc, argv);


  return 0;
};
}


