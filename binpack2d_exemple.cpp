#include "binpack2d.hpp"

#include <stdio.h>

 // Your data - whatever you want to associate with 'rectangle'
 class MyContent {
  public:
  std::string str;
  MyContent() : str("default string") {}
  MyContent(const std::string &str) : str(str) {}
  };
 
 int ExampleProgram() {
   
  srandom(0x69);
    
  // Create some 'content' to work on.
  BinPack2D::ContentAccumulator<MyContent> inputContent;
  
  for(int i=0;i<20;i++) {
   
    // random size for this content
    int width  = ((random() % 32)+1) * ((random() % 10)+1);
    int height = ((random() % 32)+1) * ((random() % 10)+1);
    
    // whatever data you want to associate with this content
    std::stringstream ss;
    ss << "box " << i;
    MyContent mycontent( ss.str().c_str() );
    
    // Add it
    inputContent += BinPack2D::Content<MyContent>(mycontent, BinPack2D::Coord(), BinPack2D::Size(width, height), false );
  }
  
  // Sort the input content by size... usually packs better.
  inputContent.Sort();
  
  // Create some bins! ( 2 bins, 128x128 in this example )
  BinPack2D::CanvasArray<MyContent> canvasArray = 
    BinPack2D::UniformCanvasArrayBuilder<MyContent>(128,128,2).Build();
    
  // A place to store content that didnt fit into the canvas array.
  BinPack2D::ContentAccumulator<MyContent> remainder;
  
  // try to pack content into the bins.
  bool success = canvasArray.Place( inputContent, remainder );
  
  // A place to store packed content.
  BinPack2D::ContentAccumulator<MyContent> outputContent;
  
  // Read all placed content.
  canvasArray.CollectContent( outputContent );
  
  // parse output.
  typedef BinPack2D::Content<MyContent>::Vector::iterator binpack2d_iterator;
  printf("PLACED:\n");
  for( binpack2d_iterator itor = outputContent.Get().begin(); itor != outputContent.Get().end(); itor++ ) {
   
    const BinPack2D::Content<MyContent> &content = *itor;
    
    // retreive your data.
    const MyContent &myContent = content.content;
  
    printf("\t%9s of size %3dx%3d at position %3d,%3d,%2d rotated=%s\n",
	   myContent.str.c_str(), 
	   content.size.w, 
	   content.size.h, 
	   content.coord.x, 
	   content.coord.y, 
	   content.coord.z, 
	   (content.rotated ? "yes":" no"));
  }
  
  printf("NOT PLACED:\n");
  for( binpack2d_iterator itor = remainder.Get().begin(); itor != remainder.Get().end(); itor++ ) {
   
    const BinPack2D::Content<MyContent> &content = *itor;
    
    const MyContent &myContent = content.content;
  
    printf("\t%9s of size %3dx%3d\n",
	   myContent.str.c_str(), 
	   content.size.w, 
	   content.size.h);
  }
}

int main(int argc, char** argv)
{
    ExampleProgram();
    return 0;
}
