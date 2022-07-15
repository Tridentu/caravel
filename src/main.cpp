
#include <caravel/CaravelWriter.h>
#include <caravel/CaravelUtils.h>
#include <caravel/CaravelReader.h>
#include <caravel/CaravelAuthor.h>
#include <caravel/CaravelContext.h>
#include <caravel/CaravelDownloader.h>
#include <caravel/CaravelDBContext.h>
#include <caravel/CaravelSigner.hpp>
#include <caravel/CaravelPackageChecker.hpp>
#include <string>
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <CLI11.hpp>
#include <vector>
#include <mast_tk/core/LineUtils.hpp>
#include <caravel/CaravelTypeLoader.hpp>



CaravelPM::CaravelTypeLoader* loader = CaravelPM::InitLoader();


struct CaravelPkgTypeValidator : public CLI::Validator {
    CaravelPkgTypeValidator(){
      name_ = "CRAVPKGTYPE";
      func_ = [&](const std::string& str){
          if(loader->getPackageType(str).name.empty()){
            return std::string("the package type must be one of the types registered in /usr/lib/caravel-plugins."); 
          } else {
            return std::string();
          }
      };
    }
};

int main(int argc, char** argv){

  CLI::App caravelApp;
  
  
    loader->loadAllTypes();
  caravelApp.require_subcommand(1);

  {
    auto createPackage = caravelApp.add_subcommand("create-package","Creates a .caravel package for distribution using the given folder.");

    std::string lua_script_install, lua_script_uninstall;
    std::string packageDir;
    createPackage->add_option("packagedir",packageDir,"The package directory (relative to the current working directory)")->required();

    std::string packageType;
    createPackage->add_option("type",packageType,"The type of Caravel package to create (binaries, source, config or assets).")->required()->check(CaravelPkgTypeValidator());


    bool hybrid_only;
    createPackage->add_flag("--hybrid", hybrid_only, "Whether or not the package is reinstallable.");


    createPackage->callback([&](){
      std::filesystem::path path_pkg = ((std::filesystem::current_path() +=  "/") += packageDir);
      if (!std::filesystem::exists(path_pkg)){
        std::cout << "Package Directory does not exist." << std::endl;
        return 0;
      }
      std::map<std::string, std::string> propMap;

      if(hybrid_only){
        propMap.insert(std::make_pair("buildType","hybrid"));
      } else
          propMap.insert(std::make_pair("buildType","regular"));


      std::cout << "Creating package..." << std::endl;
      CaravelPM::CaravelPackageType packageType2 = loader->getPackageType(packageType);
      if(!packageType2.name.empty()){
          std::cout <<  packageType2.name <<  " format detected." << std::endl;
          CaravelPM::CaravelAuthor::CreatePackage(packageDir,packageType2.name,propMap,loader);
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
    bool local_package = false;
    auto cb = [&](int count){ local_package = (count > 0);};
    CLI::Option* localPackage = installPackage->add_flag("-l,--local",cb,"Uses a local file instead of a package in a repository.");

    std::string packageName;
    installPackage->add_option("packagename", packageName, "The name of the package (or archive) to install.")->required();
  
    installPackage->callback([&](){
      CaravelPM::CaravelDBContext::InitDB("https://tridentu.github.io/acquirium/pman.caraveldb");
      if(local_package){
        std::filesystem::path path_pkg = std::filesystem::current_path() / std::filesystem::path(std::string(packageName + ".caravel"));
        CaravelPM::CaravelReader* reader = new CaravelPM::CaravelReader(path_pkg.string(),std::string(packageName + ".caravel"), loader);

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
          CaravelPM::CaravelPackageGroup*  pkgGroup;
          if(packageNamespace.empty()){
            pkgGroup = CaravelPM::CaravelDBContext::GetDB()->GetPackageGroup(packageName);
          }
          
          if(packageNamespace.empty() && !pkgGroup){
            std::cerr << "Can't download package; " << packageName << " doesn't exist." << std::endl;
            return 0;
          } else if(pkgGroup && packageNamespace.empty()){
            std::cout << "Found package: " << pkgGroup->ToPackage() << std::endl;
            packageName = pkgGroup->ToPackage();
          }
          std::cout << "Downloading Caravel Package " << packageName << "..." << std::endl;
          std::string url = CaravelPM::CaravelDBContext::GetDB()->GetPackageLink(packageName);
          CaravelPM::CaravelDownloader* downloader = new CaravelPM::CaravelDownloader(packageName, url);

          downloader->Run();
          std::cout << "Installing Caravel Package " << packageName << "..." << std::endl;
          
          std::filesystem::path path_pkg = std::filesystem::path(std::string(getenv("HOME")) + std::string("/" + packageName + ".caravel"));
          
          CaravelPM::CaravelPackageChecker* checker = new CaravelPM::CaravelPackageChecker(path_pkg.string(), true, packageName);
          std::cout << "Loading signature file..." << std::endl;
          std::string packageType = CaravelPM::CaravelDBContext::GetDB()->FindType(packageName);

          auto packageTypeObj = loader->getPackageType(packageType);
          checker->LoadSignatureAndContents(true,packageTypeObj.ver_dir());
          std::cout << "Verifying package..." << std::endl;

          bool isVerified = checker->Verify();
          if (!isVerified){
                std::cerr << "Can't extract package - marked as malicious or unknown. Exiting..." << std::endl;
                return 0;
          }
          delete checker;
          CaravelPM::CaravelReader* reader = new CaravelPM::CaravelReader(path_pkg.string(),std::string(packageName + ".caravel"), loader);
          if(!reader->Extract()){
            std::cerr << "Can't extract package." << std::endl;
            return 0;
          }
          if(reader->hasHybridType()){
             std::cout << "Hybrid package detected. Caravel will reinstall this package." << std::endl;
             std::string uninstallScript(std::string(std::string(getenv("HOME")) + "/ccontainer/uninstall.lua"));
             std::filesystem::path ulPath(uninstallScript);

             CaravelPM::CaravelContext* context = new CaravelPM::CaravelContext(ulPath.string());
             context->Run();
             delete context;
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
  {
    auto signPackages = caravelApp.add_subcommand("sign-package","Signs the given caravel package");
    
    std::string packageName;
    signPackages->add_option("package", packageName, "The package to sign.")->required();
    signPackages->callback([&](){
        std::filesystem::path path_pkg = std::filesystem::current_path() / std::filesystem::path(std::string(packageName + ".caravel"));
        CaravelPM::CaravelSigner* signer = new CaravelPM::CaravelSigner(path_pkg.string());
        std::cout << "Initializing..." << std::endl;
        signer->SetProtocol(CaravelPM::CaravelSigningProtocol::OpenPGP);
        std::vector<std::string> signerChoices;
        signerChoices.push_back("1");
        signerChoices.push_back("2");
        signerChoices.push_back("3");
        signerChoices.push_back("4");

        int signerCount = MastTDE::LineIO::GetChoice("How many signers do you need?", signerChoices) + 1;
        for (int i = 0; i < signerCount; ++i){
            bool useEK = MastTDE::LineIO::Confirm("Use an existing key?");
            std::string address = MastTDE::LineIO::Prompt("Enter Signer #" + std::to_string(i + 1) + "'s email address: ");
            if (!address.empty())
                signer->AddSigner(address, true, useEK);
            else {
                std::cerr << "Can't sign package: email address not provided." << std::endl;
                return 0;
            }
        }
        std::cout << "Signing the package..." << std::endl;
        signer->SignFile(path_pkg.string() + ".sig", CaravelPM::CaravelSigningMode::Detach);
        std::cout << "Package signed." << std::endl;
        sleep(3);
        return 0;
    });
  }
    {
    auto list_ip = caravelApp.add_subcommand("list-installed-packages","Lists all installed packages");
 
    list_ip->callback([&](){
        CaravelPM::CaravelDBContext::InitDB("https://tridentu.github.io/acquirium/pman.caraveldb");
        auto packages =  CaravelPM::CaravelDBContext::GetDB()->GetInstalledPackages();
        if (packages.size() <= 0)
            std::cout << "No packages installed." << std::endl;
        else {
            for(const auto& pack : packages)
                std::cout << pack.name << std::endl;
        }
            
        return 0;
    });
  }
  caravelApp.footer("Caravel v0.3.0");
  
  CLI11_PARSE(caravelApp, argc, argv);


  return 0;
};
}


