/////////////////////////////////////////////////
//// integrator /////////////////////////////////
/////////////////////////////////////////////////
module DDA (
	input clk,
	input reset,
	output  signed [26:0] xnew,
	output  signed [26:0] ynew,
	output  signed [26:0] znew,
	output  [8:0] overflow
);

	reg signed [26:0] x0={7'b1111111,20'b0};
	reg signed [26:0] y0=27'd104857;
	reg signed [26:0] z0=27'd26214400;
	reg signed [26:0] dt=27'd4096;
	reg signed [26:0] sigma=27'd10485760;
	reg signed [26:0] beta=27'd2796202;
	reg signed [26:0] rho=27'd29360128;
	wire signed [26:0] input_x;
	wire signed [26:0] input_y;
	wire signed [26:0] input_z;
	wire signed [26:0] mul1;
	wire signed [26:0] mul2;
	wire signed [26:0] mul3;
	wire signed [26:0] mul_res2;
	wire signed [26:0] mul_res3;
	wire signed [26:0] mul_res5;
	reg signed [26:0] xmux, ymux, zmux;
	wire [26:0] mul_res [8:0];
	always @ (*) begin
		if(reset) begin
			xmux =x0;
			ymux =y0;
			zmux =z0;
		end
		else begin
			xmux =xnew;
			ymux =ynew;
			zmux =znew;			
		end
	end
	/*
	signed_mult inst0 (
		.out(mul1),
		.a(ymux-xmux),
		.b(sigma),
		.overflow(overflow[0])
	);
	signed_mult inst1 (
		.out(input_x),
		.a(mul1),
		.b(dt),
		.overflow(overflow[1])
	);
	signed_mult inst2 (
		.out(mul_res2),
		.a(xmux),
		.b(ymux),
		.overflow(overflow[2])
	);
	signed_mult inst3 (
		.out(mul_res3),
		.a(rho-zmux),
		.b(xmux),
		.overflow(overflow[3])
	);
	signed_mult inst4 (
		.out(input_y),
		.a(mul_res3-ymux),
		.b(dt),
		.overflow(overflow[4])
	);
	signed_mult inst5 (
		.out(mul_res5),
		.a(zmux),
		.b(beta),
		.overflow(overflow[5])
	);
	signed_mult inst6 (
		.out(input_z),
		.a(mul_res2-mul_res5),
		.b(dt),
		.overflow(overflow[6])
	);
	*/
	/*
	signed_mult inst0 (
		.out(mul_res[0]),
		.a(dt),
		.b(sigma),
		.overflow(overflow[0])
	);
*/
	assign mul_res[0] = sigma >>> 8 ;
	
	signed_mult inst1 (
		.out(input_x),
		.a(mul_res[0]),
		.b(ymux-xmux),
		.overflow(overflow[1])
	);



	/*signed_mult inst2 (
		.out(mul_res[2]),
		.a(dt),
		.b(rho-zmux),
		.overflow(overflow[2])
	);*/
	assign mul_res[2] = (rho-zmux) >>> 8;
	signed_mult inst3 (
		.out(mul_res[3]),
		.a(mul_res[2]),
		.b(xmux),
		.overflow(overflow[3])
	);
/*
	signed_mult inst4 (
		.out(mul_res[4]),
		.a(dt),
		.b(ymux),
		.overflow(overflow[4])
	);*/
	assign mul_res[4] = (ymux) >>>8;
	assign input_y = mul_res[3] - mul_res[4];


	/*
	signed_mult inst5 (
		.out(mul_res[5]),
		.a(dt),
		.b(xmux),
		.overflow(overflow[5])
	);*/
	assign mul_res[5] = (xmux) >>>8;
	signed_mult inst6 (
		.out(mul_res[6]),
		.a(mul_res[5]),
		.b(ymux),
		.overflow(overflow[6])
	);
/*
	signed_mult inst7 (
		.out(mul_res[7]),
		.a(dt),
		.b(zmux),
		.overflow(overflow[7])
	);*/
	assign mul_res[7] = (zmux) >>>8;
	signed_mult inst8 (
		.out(mul_res[8]),
		.a(mul_res[7]),
		.b(beta),
		.overflow(overflow[8])
	);
	assign input_z = mul_res[6]-mul_res[8];


	integrator x(
		.out(xnew),
		.funct(input_x),
		.InitialOut(x0+input_x),
		.clk(clk),
		.reset(reset)
	);
	integrator y(
		.out(ynew),
		.funct(input_y),
		.InitialOut(y0+input_y),
		.clk(clk),
		.reset(reset)
	);
	integrator z(
		.out(znew),
		.funct(input_z),
		.InitialOut(z0+input_z),
		.clk(clk),
		.reset(reset)
	);

endmodule

module integrator(out,funct,InitialOut,clk,reset);
	output signed [26:0] out; 		//the state variable V
	input signed [26:0] funct;      //the dV/dt function
	input clk, reset;
	input signed [26:0] InitialOut;  //the initial state variable V
	
	wire signed	[26:0] out, v1new ;
	reg signed	[26:0] v1 ;
	
	always @ (posedge clk) 
	begin
		if (reset==1) //reset	
			v1 <= InitialOut ; // 
		else 
			v1 <= v1new ;	
	end
	assign v1new = v1 + funct ;
	assign out = v1 ;
endmodule

//////////////////////////////////////////////////
//// signed mult of 7.20 format 2'comp////////////
//////////////////////////////////////////////////

module signed_mult (out, a, b,overflow);
	output 	signed  [26:0]	out;
	output  signed  overflow;
	input 	signed	[26:0] 	a;
	input 	signed	[26:0] 	b;
	// intermediate full bit length
	wire 	signed	[53:0]	mult_out;
	assign mult_out = a * b;
	// select bits for 7.20 fixed point
	assign out = {mult_out[53], mult_out[45:20]};

    assign overflow = (mult_out[52:46]!=7'b0);
endmodule
//////////////////////////////////////////////////