#include <iostream>
#include <fstream>
#include <sstream>
#include <stdio.h>     
#include <stdlib.h>
#include <cstdlib>
#include <vector>
#include <string>
#include <algorithm>
#include <unordered_map>

#include "TTree.h"
#include "TH1D.h"
#include "TFile.h"
#include "TMath.h"
#include "TCanvas.h"
#include "TSpline.h"

#include "WCSimRootOptions.hh"
//#include "WCSimRootGeom.hh"
#include "WCSimRootEvent.hh"
#include "G4ThreeVector.hh"

#include "check_uniform_voxel.hh"

using namespace std;
//unbinned KS test on the uniformity of generated data points
double KS_test(std::vector<double> testvec, double start, double stop){
  
  sort(testvec.begin(), testvec.end());
  int npoints = (int)testvec.size();

  vector<double> nullvec(npoints);
  double step = (stop - start) / npoints;
  for (int i = 0; i < npoints ; i++){
    nullvec[i] = start + (i + 0.5) * step;
  }
  // Convert vector<double> to double arrays
  double* nullarr = nullvec.data();
  double* testarr = testvec.data();
  
  double KS_prob  = TMath::KolmogorovTest(npoints, nullarr, npoints, testarr,"");

  return KS_prob;    
}

double Chi2_test(TH1D *hbase, TH1D *h){
  double p = hbase->Chi2Test(h, "WW");
  return p;
}

double ComputeKLDivergence(TH1D* h1, TH1D* h2) {
    // Ensure both histograms are normalized
    h1->Scale(1.0 / h1->Integral());
    h2->Scale(1.0 / h2->Integral());

    double klDiv = 0.0;
    int nBins = h1->GetNbinsX();

    for (int i = 1; i <= nBins; ++i) {
        double p = h1->GetBinContent(i) + 1.E-8;
        double q = h2->GetBinContent(i) + 1.E-8;

        if (p > 0 && q > 0) {
            klDiv += p * TMath::Log(p / q);
        }
    }

    return klDiv;
}


void Fill_Lambda_hSpec(){
  for (int i = 0; i < nGammaOutcomes+1; i++){
    hist_binedges[i] = gammaWavelengths[i] - wavelength_binwidth/2.;
  }
  hist_binedges[nGammaOutcomes] = gammaWavelengths[nGammaOutcomes-1] + wavelength_binwidth/2.;
  hSpec = new TH1D("hSpec", "hSpec", nGammaOutcomes, hist_binedges[0], hist_binedges[nGammaOutcomes]);
  for (int i = 0; i < nGammaOutcomes; i++){
    hSpec->SetBinContent(i+1, gammaSpectrum[i] / hSpec->GetBinCenter(i+1));
  }
  hSpec->Scale(1./hSpec->Integral("width"));
  hSpec->SetDirectory(0);
}

void Load_Voxel_Definition(const char* filename){
  ifstream voxfile(filename);
  if (!voxfile.is_open()) {
    cerr << "Failed to open file: " << voxdefname << endl;
    exit(-999);
  }

  std::string line;
  while (std::getline(voxfile, line)) {
    std::string key;
    std::string value;
    // Find the position of the delimiter ':'
    size_t delimiterPos = line.find(':');
    if (delimiterPos != std::string::npos) {
      // Extract the key (before ':')
      key = line.substr(0, delimiterPos);
      // Extract the value (after ':')
      value = line.substr(delimiterPos + 1);
      
      // Trim leading and trailing whitespace from key and value
      key = key.substr(key.find_first_not_of(" \t"));
      value = value.substr(value.find_first_not_of(" \t"));
      
      // Optionally trim trailing whitespace from value
      value = value.substr(0, value.find_last_not_of(" \t") + 1);
      
      //std::cout << "Key: '" << key << "'" << std::endl;
      //std::cout << "Value: '" << value << "'" << std::endl;
      if (std::find(keys.begin(), keys.end(), key) == keys.end()){
	cerr << "Given key " << key << " does not exist in the list nor match with the names. Please check input config macros." << endl;
	exit(-99);
      }

      variables[key] = std::stod(value);      
    } else {
      std::cerr << "Invalid line format: " << line << std::endl;
    }
  }
  
  voxfile.close();

}

int main(int argc, char *argv[])
{

  for (int i = 0; i < argc; i++){
    if((strcmp(argv[i],"-f")==0) && i+1<argc){
      i++;
      if(rootfilename!=NULL) delete[] rootfilename;
      rootfilename = new char[strlen(argv[i])+1];
      strcpy(rootfilename,argv[i]);
      std::cout<<"Read in WCSim root file: "<<rootfilename<<std::endl;
      continue;
    }

    if((strcmp(argv[i],"-c")==0) && i+1<argc){
      i++;
      if(voxdefname!=NULL) delete[] voxdefname;
      voxdefname = new char[strlen(argv[i])+1];
      strcpy(voxdefname,argv[i]);
      std::cout<<"Read in config file for voxel defination: "<<voxdefname<<std::endl;
      continue;
    }

    if(((strcmp(argv[i],"-x")==0) || strcmp(argv[i], "--x2") == 0)  && i<argc){
      i++;
      all_Chi2 = 1;
      std::cout<<"All using Chi2 test! "<<std::endl;
      continue;
    }

    if(((strcmp(argv[i],"-v")==0) || strcmp(argv[i], "--verbose") == 0)  && i<argc){
      i++;
      verbose = 1;
      std::cout<<"Verbose mode! "<<std::endl;
      continue;
    }

    if(((strcmp(argv[i],"-h")==0) || strcmp(argv[i], "--help") == 0)  && i<argc){
      i++;
      print_usage = 1;      
      break;	
    }
  }

  if( rootfilename==NULL || voxdefname==NULL || print_usage ){
    cout <<"Usage:"<<endl;
    cout << "check_uniform_voxel -f <wcsim.root> -c <voxel_definition.mac> [-x (for using Chi2 test on all distributions), not recommended] [-v (for verbose mode)]"<<endl;
    exit(-1);
  }

  //read the config file
  Load_Voxel_Definition(voxdefname);
  Fill_Lambda_hSpec();
  TH1D *hlambda = (TH1D*)hSpec->Clone();
  hlambda->SetDirectory(0);
  hlambda->Clear();
    
  // Open the file
  TFile * file = new TFile(rootfilename,"read");
  if (!file->IsOpen()){
    std::cout << "Error, could not open input file: " << rootfilename << std::endl;
    return -1;
  }

  std::vector<double> vec_r;
  std::vector<double> vec_cosp;
  std::vector<double> vec_z;
  std::vector<double> vec_dirp;
  std::vector<double> vec_cosz;
  
  // Get the a pointer to the tree from the file
  TTree *tree = (TTree*)file->Get("wcsimT");
  
  // Get the number of events
  const long nevent = tree->GetEntries();
  if(verbose) printf("Number of Event Tree Entries: %ld\n",nevent);
  
  // Create a WCSimRootEvent to put stuff from the tree in
  WCSimRootEvent* wcsimrootsuperevent = new WCSimRootEvent();

  // Set the branch address for reading from the tree
  TBranch *branch = tree->GetBranch("wcsimrootevent");
  branch->SetAddress(&wcsimrootsuperevent);

  // Force deletion to prevent memory leak 
  tree->GetBranch("wcsimrootevent")->SetAutoDelete(kTRUE);

  // start with the main "subevent", as it contains most of the info
  // and always exists.
  WCSimRootTrigger* wcsimrootevent;  
  // Now loop over events
  for (long ievent=0; ievent<nevent; ievent++)
  {
    // Read the event from the tree into the WCSimRootEvent instance
    tree->GetEntry(ievent);      
    wcsimrootevent = wcsimrootsuperevent->GetTrigger(0);
    //setvtx in wcsim converts mm to cm, so an offset of 10
    double VtxX = wcsimrootevent->GetVtx(0)*10.;
    double VtxY = wcsimrootevent->GetVtx(1)*10.;
    double VtxZ = wcsimrootevent->GetVtx(2)*10.;

    double R = sqrt(VtxX*VtxX + VtxZ*VtxZ);
    double cosPhi = VtxX/R;

    vec_r.push_back(R);
    vec_cosp.push_back(cosPhi);
    vec_z.push_back(VtxY);
    
    // Now read the tracks in the event
    // Loop through elements in the TClonesArray of WCSimTracks
    for (int itrack=0; itrack<1; itrack++)
    {
      TObject *element = (wcsimrootevent->GetTracks())->At(itrack);
      if(!element)
	continue;
      WCSimRootTrack *wcsimroottrack = dynamic_cast<WCSimRootTrack*>(element);

      double dirX = wcsimroottrack->GetDir(0);
      double dirY = wcsimroottrack->GetDir(1);
      double dirZ = wcsimroottrack->GetDir(2);

      G4ThreeVector dir = G4ThreeVector(dirX, -dirZ, dirY);
      double cosZ = std::cos(dir.theta());
      double dirP = dir.phi()*360./2/TMath::Pi() + 180.;
      
      double eventE = wcsimroottrack->GetE();
      double lambda = 1.240E-3/eventE;

      hlambda->Fill(lambda);

      vec_dirp.push_back(dirP);
      vec_cosz.push_back(cosZ);
	
    }  // itrack // End of loop over tracks

    // Now look at the Cherenkov hits
    
    int ncherenkovhits     = wcsimrootevent->GetNcherenkovhits();
    int ncherenkovdigihits = wcsimrootevent->GetNcherenkovdigihits(); 
    
    if(verbose){
      cout << "RAW HITS:" << endl;
      printf("Number of PMTs with a true hit %d\n",     ncherenkovhits);
    }

    wcsimrootsuperevent->ReInitialize();
    
  } // ievent // End of loop over events
  
  hlambda->Scale(1./hlambda->Integral("width"));
  //delete hlambda;
  //delete hSpectrum;
  //file->Close();

  int output = 1;

  if (all_Chi2){
    check_values["E_spectrum"] = Chi2_test(hSpec, hlambda);
  }
  else{
    check_values["E_spectrum"] = 1. - ComputeKLDivergence(hlambda, hSpec);
    check_values["vtx_r"] = KS_test(vec_r, variables["r0"], variables["r1"]);
    check_values["vtx_cosphi"] = KS_test(vec_cosp, variables["phi0"], variables["phi1"]);
    check_values["vtx_z"] = KS_test(vec_z, variables["z0"], variables["z1"]);
    check_values["dirphi"] = KS_test(vec_dirp, 0., 360.);
    check_values["cosz"] = KS_test(vec_cosz, -1., 1.);        
  }
  
  for (auto i = check_values.begin(); i != check_values.end(); i++){
    output *= (i->second > variables["criterion"] ? 1.: 0.);
    std::cout << i->first << " = " << i->second << std::endl;
    if (output < 1){
      cerr << i->first << " has failed because the checked value " << i->second << " is less than the threshold " << variables["criterion"] << endl;
      return output;
    }    
  }
  /*
  TCanvas *c = new TCanvas("c","",800,800);
  c->cd();
  hlambda->SetLineColor(2);
  hlambda->Draw("hist");
  hSpec->Draw("same hist");
  TSpline3* spline = new TSpline3(hSpec,"", hist_binedges[0], hist_binedges[23]);
  spline->SetLineColor(6);
  spline->Draw("same");
  //c->SetLogy(1);
  c->Modified();
  c->Update();
  c->Print("test_hSpec_hlambda.png");
  */
  delete hSpec;
  delete hlambda;
  return output;
}
